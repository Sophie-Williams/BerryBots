/*
  Copyright (C) 2013-2015 - Voidious

  This software is provided 'as-is', without any express or implied
  warranty.  In no event will the authors be held liable for any damages
  arising from the use of this software.

  Permission is granted to anyone to use this software for any purpose,
  including commercial applications, and to alter it and redistribute it
  freely, subject to the following restrictions:

  1. The origin of this software must not be misrepresented; you must not
     claim that you wrote the original software. If you use this software
     in a product, an acknowledgment in the product documentation would be
     appreciated but is not required.
  2. Altered source versions must be plainly marked as such, and must not be
     misrepresented as being the original software.
  3. This notice may not be removed or altered from any source distribution.
*/

#ifndef DOCK_SHAPE_H
#define DOCK_SHAPE_H

#include <SFML/Graphics.hpp>
#include "dockitem.h"

class DockShape : public DockItem {
  sf::Shape **drawableShapes_;
  sf::Text *shortcutText_;

  public:
    DockShape(sf::Shape **shapes, int numShapes, int left, int top, int width,
              int height, const char *hoverText, sf::Font *font, int fontSize,
              int textLeft, int textTop, const char *shortcut,
              int shortcutFontSize);
    ~DockShape();
    void setTop(int top, int textTop);
    virtual void setHighlighted(bool highlighted);
};

#endif
