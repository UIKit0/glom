/* Glom
 *
 * Copyright (C) 2007 Murray Cumming
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of the
 * License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

#ifndef GLOM_UTILITY_WIDGETS_CANVAS_RECT_MOVABLE_H
#define GLOM_UTILITY_WIDGETS_CANVAS_RECT_MOVABLE_H

#include "canvas_item_movable.h"
#include <goocanvasmm/rect.h>

namespace Glom
{

class CanvasRectMovable
  : public Goocanvas::Rect,
    public CanvasItemMovable
{
private:
  CanvasRectMovable();
  CanvasRectMovable(double x, double y, double width, double height);
  virtual ~CanvasRectMovable();

  void init();

public:
  static Glib::RefPtr<CanvasRectMovable> create();
  static Glib::RefPtr<CanvasRectMovable> create(double x, double y, double width, double height);

  enum Corners
  {
    CORNER_ALL, // Snap to all corners.
    CORNER_TOP_LEFT,
    CORNER_TOP_RIGHT,
    CORNER_BOTTOM_LEFT,
    CORNER_BOTTOM_RIGHT,
    CORNER_COUNT
  };

  /** Specify the corner to be considered when snapping to a grid while moving.
   */
  void set_snap_corner(Corners corner);

  virtual void get_xy(double& x, double& y) const;
  virtual void set_xy(double x, double y);
  virtual void get_width_height(double& width, double& height) const;
  virtual void set_width_height(double width, double height);

private:
  virtual Goocanvas::Canvas* get_parent_canvas_widget();

  virtual void snap_position(double& x, double& y) const;
  void snap_position_one_corner(Corners corner, double& x, double& y) const;
  void snap_position_all_corners(double& x, double& y) const;


  //What corner is considered when snapping to a grid while moving:
  Corners m_snap_corner;
};

} //namespace Glom

#endif //GLOM_UTILITY_WIDGETS_CANVAS_RECT_MOVABLE_H

