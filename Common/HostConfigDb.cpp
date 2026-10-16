/*
 *   Update created by Rick KD0OSS for OpenMT  2026
 *
 *   This update to ircDDB allows for using OpenMT's database to store
 *   and retrieve it's settings.
 * 
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "HostConfigDb.h"

#include <wx/textfile.h>

#if defined(__has_include)
#if __has_include(<mariadb/mysql.h>)
#include <mariadb/mysql.h>
#else
#include <mysql/mysql.h>
#endif
#else
#include <mariadb/mysql.h>
#endif

#include <string>

static const char* OPENMT_INI    = "/etc/openmt.ini";
static const char* DB_USER       = "openmt";
static const char* DEFAULT_MODEM = "modem1";

static std::string toStd(const wxString& s)
{
	wxCharBuffer buf = s.mb_str(wxConvUTF8);
	return std::string(buf.data() != NULL ? buf.data() : "");
}

// The same lookup as readConfig() in the OpenMT tools: key in a [section]
static wxString readOpenMTIni(const wxString& section, const wxString& key, const wxString& def)
{
	wxTextFile file(OPENMT_INI);

	if (!file.Exists() || !file.Open())
		return def;

	wxString current;
	wxString result = def;

	for (size_t i = 0U; i < file.GetLineCount(); i++) {
		wxString line = file.GetLine(i);
		line.Trim(true).Trim(false);

		if (line.IsEmpty() || line[0U] == wxT(';') || line[0U] == wxT('#'))
			continue;

		if (line[0U] == wxT('[') && line.Last() == wxT(']')) {
			current = line.Mid(1U, line.Len() - 2U);
			continue;
		}

		if (!current.IsSameAs(section))
			continue;

		int n = line.Find(wxT('='));
		if (n == wxNOT_FOUND)
			continue;

		wxString k = line.Left(n);
		k.Trim(true).Trim(false);

		if (k.IsSameAs(key)) {
			result = line.Mid(n + 1U);
			result.Trim(true).Trim(false);
			break;
		}
	}

	file.Close();

	return result;
}

static MYSQL* openDatabase()
{
	std::string host   = toStd(readOpenMTIni(wxT("database"), wxT("host"), wxT("localhost")));
	std::string passwd = toStd(readOpenMTIni(wxT("database"), wxT("passwd"), wxEmptyString));

	MYSQL* conn = ::mysql_init(NULL);
	if (conn == NULL) {
		wxLogError(wxT("HostConfigDb: mysql_init() failed"));
		return NULL;
	}

	// CLIENT_FOUND_ROWS: an UPDATE that sets the value it already has still counts as a match
	if (::mysql_real_connect(conn, host.c_str(), DB_USER, passwd.c_str(), "", 0, NULL, CLIENT_FOUND_ROWS) == NULL) {
		wxLogError(wxT("HostConfigDb: cannot connect to the database on %s: %s"), wxString(host.c_str(), wxConvUTF8).c_str(), wxString(::mysql_error(conn), wxConvUTF8).c_str());
		::mysql_close(conn);
		return NULL;
	}

	::mysql_set_character_set(conn, "utf8mb4");

	return conn;
}

static std::string escape(MYSQL* conn, const std::string& in)
{
	std::string out(in.size() * 2U + 1U, '\0');
	unsigned long n = ::mysql_real_escape_string(conn, &out[0], in.c_str(), (unsigned long)in.size());
	out.resize(n);
	return out;
}

static bool run(MYSQL* conn, const std::string& query)
{
	if (::mysql_real_query(conn, query.c_str(), (unsigned long)query.size()) != 0) {
		wxLogError(wxT("HostConfigDb: query failed: %s"), wxString(::mysql_error(conn), wxConvUTF8).c_str());
		return false;
	}

	return true;
}

wxString CHostConfigDb::modemName(const wxString& name)
{
	return name.IsEmpty() ? wxString(DEFAULT_MODEM, wxConvUTF8) : name;
}

bool CHostConfigDb::load(const wxString& modem, const wxString& module, const wxString& oldFile, wxArrayString& lines)
{
	lines.Clear();

	MYSQL* conn = openDatabase();
	if (conn == NULL)
		return false;

	std::string query = "SELECT parameter, value FROM modem_host.config WHERE modem_name = '" + escape(conn, toStd(modem)) +
	                    "' AND module = '" + escape(conn, toStd(module)) + "'";

	if (!run(conn, query)) {
		::mysql_close(conn);
		return false;
	}

	MYSQL_RES* result = ::mysql_store_result(conn);
	if (result == NULL) {
		wxLogError(wxT("HostConfigDb: cannot read the settings: %s"), wxString(::mysql_error(conn), wxConvUTF8).c_str());
		::mysql_close(conn);
		return false;
	}

	MYSQL_ROW row;
	while ((row = ::mysql_fetch_row(result)) != NULL) {
		if (row[0] == NULL)
			continue;

		wxString key(row[0], wxConvUTF8);
		wxString val(row[1] != NULL ? row[1] : "", wxConvUTF8);
		lines.Add(key + wxT("=") + val);
	}

	::mysql_free_result(result);
	::mysql_close(conn);

	// Nothing stored yet: take over the settings of the old file
	if (lines.IsEmpty() && !oldFile.IsEmpty()) {
		wxTextFile file(oldFile);

		if (file.Exists() && file.Open()) {
			for (size_t i = 0U; i < file.GetLineCount(); i++) {
				wxString line = file.GetLine(i);

				if (line.IsEmpty() || line[0U] == wxT('#') || line.Find(wxT('=')) == wxNOT_FOUND)
					continue;

				lines.Add(line);
			}

			file.Close();

			if (!lines.IsEmpty()) {
				if (save(modem, module, lines))
					wxLogMessage(wxT("HostConfigDb: the settings of %s are now in the database"), oldFile.c_str());
				else
					wxLogError(wxT("HostConfigDb: cannot store the settings of %s in the database"), oldFile.c_str());
			}
		}
	}

	return true;
}

bool CHostConfigDb::save(const wxString& modem, const wxString& module, const wxArrayString& lines)
{
	MYSQL* conn = openDatabase();
	if (conn == NULL)
		return false;

	std::string m = escape(conn, toStd(modem));
	std::string o = escape(conn, toStd(module));

	bool ok = run(conn, "START TRANSACTION");

	for (size_t i = 0U; ok && i < lines.GetCount(); i++) {
		int n = lines[i].Find(wxT('='));
		if (n == wxNOT_FOUND)
			continue;

		std::string key = escape(conn, toStd(lines[i].Left(n)));
		std::string val = escape(conn, toStd(lines[i].Mid(n + 1U)));

		std::string where = " WHERE modem_name = '" + m + "' AND module = '" + o + "' AND parameter = '" + key + "'";

		ok = run(conn, "UPDATE modem_host.config SET value = '" + val + "'" + where);

		if (ok && ::mysql_affected_rows(conn) == 0U)
			ok = run(conn, "INSERT INTO modem_host.config SET modem_name = '" + m + "', module = '" + o + "', parameter = '" + key +
			               "', value = '" + val + "', display_type = 'misc'");
	}

	ok = run(conn, ok ? "COMMIT" : "ROLLBACK") && ok;

	::mysql_close(conn);

	return ok;
}
