/* Glom
 *
 * Copyright (C) 2001-2005 Murray Cumming
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
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef GLOM_CELLRENDERER_BUTTON_H
#define GLOM_CELLRENDERER_BUTTON_H

#include <gtkmm/cellrendererpixbuf.h>
#include <gtkmm/treepath.h>
#include <gtkmm/treemodel.h>

class GlomCellRenderer_Button : public Gtk::CellRendererPixbuf
{
public: 
  GlomCellRenderer_Button();
  virtual ~GlomCellRenderer_Button();

  typedef sigc::signal<void, const Gtk::TreeModel::Path&> type_signal_clicked;
  type_signal_clicked signal_clicked();
  
protected:
  virtual bool activate_vfunc(GdkEvent* event, Gtk::Widget& widget, const Glib::ustring& path, const Gdk::Rectangle& background_area, const Gdk::Rectangle& cell_area, Gtk::CellRendererState flags);

  type_signal_clicked m_signal_clicked;
};

#endif //GLOM_CELLRENDERER_BUTTON_H
