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
 * Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301 USA.
 */

#ifndef GLOM_DATA_BOX_DATA_CALENDAR_RELATED_H
#define GLOM_DATA_BOX_DATA_CALENDAR_RELATED_H

#include "config.h" // GLOM_ENABLE_CLIENT_ONLY

#include "box_data_portal.h"
#include <libglom/data_structure/layout/layoutitem_calendarportal.h>
#include <glom/utility_widgets/layoutwidgetbase.h>
#include <gtkmm/calendar.h>
#include <giomm/simpleactiongroup.h>

namespace Glom
{

class Dialog_Layout_Calendar_Related;

class Box_Data_Calendar_Related : public Box_Data_Portal
{
public: 
  Box_Data_Calendar_Related();
  virtual ~Box_Data_Calendar_Related();

  /**
   * @param portal: The full portal details
   */
  virtual bool init_db_details(const sharedptr<const LayoutItem_Portal>& portal, bool show_title = true);

  /** Use this if no portal is yet defined, so the user can use the context menu to define a portal.
   */
  virtual bool init_db_details(const Glib::ustring& parent_table, bool show_title = true);

private:
  virtual bool fill_from_database(); //Override.
  virtual type_vecConstLayoutFields get_fields_to_show() const; //override
  virtual void create_layout(); //override
    
    
  //Implementations of pure virtual methods from Base_DB_Table_Data:
  virtual void set_primary_key_value(const Gtk::TreeModel::iterator& row, const Gnome::Gda::Value& value);
    

  void on_record_added(const Gnome::Gda::Value& primary_key_value, const Gtk::TreeModel::iterator& row); //Override. Not a signal handler.

#ifndef GLOM_ENABLE_CLIENT_ONLY
  virtual void on_dialog_layout_hide(); //override.
#endif // !GLOM_ENABLE_CLIENT_ONLY

  virtual void enable_buttons(); //override
    
  //Implementations of pure virtual methods from Base_DB_Table_Data:
  virtual Gnome::Gda::Value get_primary_key_value_selected() const;
  virtual Gnome::Gda::Value get_primary_key_value(const Gtk::TreeModel::iterator& row) const;

#ifndef GLOM_ENABLE_CLIENT_ONLY
  virtual Dialog_Layout* create_layout_dialog() const; // override.
  virtual void prepare_layout_dialog(Dialog_Layout* dialog); // override.
#endif // !GLOM_ENABLE_CLIENT_ONLY

  Glib::ustring on_calendar_details(guint year, guint month, guint day);
  void on_calendar_month_changed();
    
  void setup_menu(Gtk::Widget* widget);
  void on_calendar_button_press_event(GdkEventButton *event);
 
  void on_MenuPopup_activate_Edit();
  void on_MenuPopup_activate_Add();
  void on_MenuPopup_activate_Delete();
    
  #ifndef GLOM_ENABLE_CLIENT_ONLY
  void on_MenuPopup_activate_layout();
  #endif
  
  void clear_cached_database_values();
    
private:
  Gtk::Calendar m_calendar;
    
  //TODO: Avoid repeating these in so many widgets:
  Gtk::Menu* m_pMenuPopup;
  Glib::RefPtr<Gio::SimpleActionGroup> m_refActionGroup;
  Glib::RefPtr<Gio::SimpleAction> m_refContextEdit, m_refContextAdd, m_refContextDelete;

#ifndef GLOM_ENABLE_CLIENT_ONLY
  Glib::RefPtr<Gio::SimpleAction> m_refContextLayout;
#endif
    
  //The cached data for the month:
  //For each date we have a list of rows (vectors):
  typedef std::vector<Gnome::Gda::Value> type_vector_values;
  typedef std::list<type_vector_values*> type_list_vectors;
  typedef std::map<Glib::Date, type_list_vectors> type_map_values;
  type_map_values m_map_values;
  mutable int m_query_column_date_field;
};

} //namespace Glom

#endif // GLOM_MODE_DATA_BOX_DATA_CALENDAR_RELATED_H
