/* Glom
 *
 * Copyright (C) 2001-2004 Murray Cumming
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

#include "dialog_formatting.h"
#include <libglom/data_structure/glomconversions.h>
#include <glom/glade_utils.h>
#include <glibmm/i18n.h>

namespace Glom
{

Dialog_Formatting::Dialog_Formatting()
: m_box_formatting(0)
{
  set_title(_("Formatting"));
  set_border_width(6);

  //GtkBuilder can't find top-level objects (GtkAdjustments in this case),
  //that one top-level object references.
  //See http://bugzilla.gnome.org/show_bug.cgi?id=575714
  //so we need to this silliness. murrayc.
  std::list<Glib::ustring> builder_ids;
  builder_ids.push_back("box_formatting");
  builder_ids.push_back("adjustment2");

  //Get the formatting stuff:
  try
  {
    Glib::RefPtr<Gtk::Builder> refXmlFormatting = Gtk::Builder::create_from_file(Utils::get_glade_file_path("glom_developer.glade"), builder_ids);
    refXmlFormatting->get_widget_derived("box_formatting", m_box_formatting);
  }
  catch(const Gtk::BuilderError& ex)
  {
    std::cerr << ex.what() << std::endl;
  }

  get_vbox()->pack_start(*m_box_formatting, Gtk::PACK_EXPAND_WIDGET);
  add_view(m_box_formatting);

  add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
  add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);

  show_all_children();
}

Dialog_Formatting::~Dialog_Formatting()
{
  remove_view(m_box_formatting);
}

void Dialog_Formatting::set_item(const sharedptr<const LayoutItem_WithFormatting>& layout_item)
{
  m_box_formatting->set_formatting(layout_item->m_formatting, false, false);

  enforce_constraints();
}

void Dialog_Formatting::use_item_chosen(const sharedptr<LayoutItem_WithFormatting>& layout_item)
{
  if(!layout_item)
    return;

  m_box_formatting->get_formatting(layout_item->m_formatting);
}

void Dialog_Formatting::enforce_constraints()
{
}

} //namespace Glom

