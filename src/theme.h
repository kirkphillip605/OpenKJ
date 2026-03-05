/*
 * Copyright (c) 2013-2024 Thomas Isaac Lightburn
 *
 * This file is part of OpenKJ.
 *
 * OpenKJ is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef THEME_H
#define THEME_H

#include <QString>
#include <QStringList>

namespace AppTheme {

// Available theme indices.
// Index 0 is always Light; the rest are dark variants.
enum ThemeIndex {
    Light        = 0,
    Dark         = 1,
    MidnightBlue = 2,
    Charcoal     = 3,
    Forest       = 4,
    Amber        = 5
};

// The default theme used when the stored preference is invalid.
static constexpr int DefaultThemeIndex = Dark;

// Returns the list of human-readable theme display names.
QStringList availableThemeNames();

// Returns the resource path to the JSON theme file for the given index.
QString themeJsonPath(int index);

// Returns true if the given theme index is a dark theme.
bool isDarkTheme(int index);

// Initializes and globally applies the Qlementine style engine
// with the theme at the given index.  Must be called after
// QApplication is constructed.
void initializeStyle(int themeIndex);

// Switches the active theme at runtime (no restart required).
void applyTheme(int themeIndex);

} // namespace AppTheme

#endif // THEME_H
