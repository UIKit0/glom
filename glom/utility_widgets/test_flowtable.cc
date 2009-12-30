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
 
#include <gtkmm.h>
#include "flowtable.h"


//#include "dragwindow.h"

/*
void on_drag_data_get_label(const Glib::RefPtr<Gdk::DragContext>&, Gtk::SelectionData& selection_data, guint, guint)
{
  selection_data.set(selection_data.get_target(), 8, (const guchar*)"label", 5);
}

void on_drag_data_get_entry(const Glib::RefPtr<Gdk::DragContext>&, Gtk::SelectionData& selection_data, guint, guint)
{
  selection_data.set(selection_data.get_target(), 8, (const guchar*)"entry", 5);
}
*/

int
main(int argc, char* argv[])
{
  Gtk::Main mainInstance(argc, argv);

  Gtk::Window window;
  //Gtk::VBox flowtable;
  Glom::FlowTable flowtable;
  flowtable.set_columns_count(3);
  flowtable.set_column_padding(100);
  flowtable.set_row_padding(0);

  Gtk::Entry button7; button7.set_text("seven");
  button7.show();
  //button7.set_size_request(100, 100);

  Gtk::Entry button8; button8.set_text("eight");
  flowtable.add(button7, button8);
  button8.show();
  //button8.set_size_request(100, 100);

  Gtk::Label button9; button9.set_text("nine"); //TODO: valgrind says that something here is leaked.
  button9.show();
  //button7.set_size_request(100, 100);

  Gtk::Entry button10; button10.set_text("ten");
  flowtable.add(button9, button10);
  button10.show();

  Gtk::Entry button11; button11.set_text("eleven");
  Gtk::Entry button12; button11.set_text("eleven");
  flowtable.add(button11, button12);
  button11.show(); button12.show();


  Gtk::Entry* button13 = Gtk::manage(new Gtk::Entry()); button13->set_text("13");
  Gtk::Entry* button14 = Gtk::manage(new Gtk::Entry()); button14->set_text("14");
  flowtable.add(*button13, *button14);
  button13->show(); button14->show();

  Gtk::Entry* button15 = new Gtk::Entry(); button15->set_text("15");
  flowtable.add(*button15);
  button15->show();
  
  window.add(flowtable);
  flowtable.set_design_mode();
  flowtable.show();

  flowtable.remove(*button13);
  flowtable.remove(*button14);
  delete button13;
  delete button14;

  // Gtk::Containers implemented in C++ can't do auto-removal-on-child-destruction 
  // because that would require a Glib::wrap() on the underlying gobj of a being-deleted instance. 
  flowtable.remove(*button15);
  delete button15;

//  Glom::DragWindow drag_window;
//  drag_window.show();
  
  Gtk::Main::run(window);

  return 0;
}





