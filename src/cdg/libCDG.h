/*
 * Copyright (c) 2013-2020 Thomas Isaac Lightburn
 *
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

 * Based on the wonderful work demystifying the CD+G spec by Jim Bumgardner in CDG Revealed
 * https://jbum.com/cdg_revealed.html
*/

#ifndef LIBCDG_H
#define LIBCDG_H

#include <QByteArray>
#include <QVector>
#include <QImage>
#include <QColor>
#include <vector>
#include <array>


namespace cdg {

/* This is the size of the display as defined by the CDG specification.
   The pixels in this region can be painted, and scrolling operations
   rotate through this number of pixels. */
const QSize FRAME_DIM_FULL= QSize(300, 216);


/* This is the size of the screen that is actually intended to be
   visible.  It is the center area of FRAME_DIM_FULL.  The remaining border
   area surrounding it is not meant to be visible. */
const QSize FRAME_DIM_CROPPED = QSize(288, 192);

// H x W + palette
const int CDG_IMAGE_SIZE = 288 * 192 + 1024;

enum TileBlockType {
    TileBlockNormal,
    TileBlockXOR
};

enum ScrollType {
    ScrollCopy,
    ScrollPreset
};

// these are statically set because the values are part of the
// CD redbook standard
enum CdgCommand : char {
    CmdMemoryPreset = 1,
    CmdBorderPreset = 2,
    CmdTileBlock = 6,
    CmdScrollPreset = 20,
    CmdScrollCopy = 24,
    CmdDefineTrans = 28,
    CmdColorsLow = 30,
    CmdColorsHigh = 31,
    CmdTileBlockXOR = 38
};

// these are also defined in the CD redbook
enum CdgColorTables : char {
    LowColors = 0,
    HighColors = 1
};

struct CDG_SubCode
{
    CdgCommand command;
    char instruction;
    std::array<char,2> parityQ;
    std::array<char,16> data;
    std::array<char,4> parityP;
};

struct CdgColorsData
{
    CdgColorsData(const std::array<char,16> &data)
    {
        int colorIdx = 0;
        for (int i=0; i < 15; i += 2)
        {
            char lowbyte = data[i];
            char highbyte = data[i + 1];
            int red{0};
            int green{0};
            int blue{0};

            if (lowbyte & 0x20) red += 8;
            if (lowbyte & 0x10) red += 4;
            if (lowbyte & 0x08) red += 2;
            if (lowbyte & 0x04) red += 1;

            if (lowbyte & 0x02) green += 8;
            if (lowbyte & 0x01) green += 4;
            if (highbyte & 0x20) green += 2;
            if (highbyte & 0x10) green += 1;

            if (highbyte & 0x08) blue += 8;
            if (highbyte & 0x04) blue += 4;
            if (highbyte & 0x02) blue += 2;
            if (highbyte & 0x01) blue += 1;

            colors[colorIdx] = QColor(red * 17, green * 17, blue * 17);
            colorIdx++;
        }
    }
    std::array<QColor,8> colors;
};

struct CdgMemoryPresetData
{
    CdgMemoryPresetData(const std::array<char,16> &data)
    {
        // use clamp to pull corruped CDG data into range
        color = std::clamp(data[0] & 0x0F, 0, 15);
        repeat = (data[1] & 0x0F);
    }
    char color;
    char repeat;
    // Only the first two bits are used, the other 14 are filler
};


struct CdgBorderPresetData
{
    CdgBorderPresetData (const std::array<char,16> &data)
    {
        // use clamp to pull corruped CDG data into range
        color = std::clamp(data[0] & 0x0F, 0, 15);
    }
    char color;
    // Only the first bit is used, the other 15 are filler
};


struct CdgTileBlockData
{
    CdgTileBlockData (const std::array<char,16> &data)
    {
        color0 = (data[0] & 0x0F);
        color1 = (data[1] & 0x0F);
        row = (data[2] & 0x1F);
        column = (data[3] & 0x3F);
        std::copy(data.begin() + 4, data.end(), tilePixels.begin());
        top  = (row    * 12);
        left = (column * 6);
    }
    char color0;
    char color1;
    char row;
    char column;
    unsigned int top;
    unsigned int left;
    std::array<char,12> tilePixels;
};

struct CdgScrollCmdData
{
    CdgScrollCmdData (const std::array<char,16> &data)
    {
        color = (data[0] & 0x0F);
        hScroll = (data[1] & 0x3F);
        vScroll = (data[2] & 0x3F);
        hSCmd = (hScroll & 0x30) >> 4;
        hSOffset = (hScroll & 0x07);
        vSCmd = (vScroll & 0x30) >> 4;
        vSOffset = (vScroll & 0x0F);
    }
    char color;
    char hScroll;
    char hSCmd;
    char hSOffset;
    char vScroll;
    char vSCmd;
    char vSOffset;
};

}


#endif // LIBCDG_H
