/*
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

#ifndef	HostConfigDb_H
#define	HostConfigDb_H

#include <wx/wx.h>

/*
 * Settings storage in the modem_host.config table (columns modem_name,
 * module, parameter, display_type, value), replacing the key=value files.
 * The database host and password come from the [database] section of
 * /etc/openmt.ini, the user is "openmt", as for the other OpenMT programs.
 *
 * A setting is one row: modem_name is the instance name (the optional name
 * on the command line, "modem1" without one), module is the program
 * ("ircDDBGateway", "TimeServer", "TimerControl") and parameter is the key.
 */
class CHostConfigDb {
public:
	// Where the settings of an instance are kept
	static wxString modemName(const wxString& name);

	// All of the settings of a module as "key=value" lines.  When the
	// database has none yet and the old file exists, it is read and its
	// content stored in the database (a one time move).  Returns false if
	// the database could not be read.
	static bool load(const wxString& modem, const wxString& module, const wxString& oldFile, wxArrayString& lines);

	// Stores "key=value" lines, adding or changing the rows.
	static bool save(const wxString& modem, const wxString& module, const wxArrayString& lines);
};

/*
 * Collects the lines of a write, the same call as wxTextFile::AddLine.
 */
class CHostConfigLines {
public:
	CHostConfigLines() : m_lines() {}

	void AddLine(const wxString& line) { m_lines.Add(line); }

	const wxArrayString& lines() const { return m_lines; }

private:
	wxArrayString m_lines;
};

#endif
