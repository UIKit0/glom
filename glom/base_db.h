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

#ifndef BASE_DB_H
#define BASE_DB_H

#include "gtkmm.h"

#include "document/document_glom.h"
#include "connectionpool.h"
#include "appstate.h"
#include "data_structure/privileges.h"
#include "data_structure/system_prefs.h"
#include "utils.h"
#include "mode_data/calcinprogress.h"
#include "bakery/View/View.h"
#include <bakery/Utilities/BusyCursor.h>

class LayoutItem_GroupBy;
class LayoutItem_Summary;
class LayoutItem_VerticalGroup;

/** A base class that is a Bakery View with some database functionality.
*/
class Base_DB :
  public View_Composite_Glom
{
public:
  Base_DB();
  virtual ~Base_DB();

  /// Specify the structure of what will be shown, and fill it.
  virtual bool init_db_details();

  /// Specify what actual data will be shown:
  virtual bool refresh_data_from_database();

  /** Returns whether we are in developer mode.
   * Some functionality will be deactivated when not in developer mode.
   */
  virtual AppState::userlevels get_userlevel() const;
  virtual void set_userlevel(AppState::userlevels value);

  static sharedptr<SharedConnection> connect_to_server();

  virtual void set_document(Document_Glom* pDocument); //View override
  virtual void load_from_document(); //View override

  typedef std::vector< sharedptr<Field> > type_vecFields;

  static type_vecFields get_fields_for_table_from_database(const Glib::ustring& table_name);

  /** Create an appropriate title for an ID string.
   * For instance, date_of_birth would become Date Of Birth.
   */
  static Glib::ustring util_title_from_string(const Glib::ustring& text);

  //This is const because const means not changing this instance, not whether we change the database.
  virtual Glib::RefPtr<Gnome::Gda::DataModel> Query_execute(const Glib::ustring& strQuery) const;

  void add_standard_groups();
  bool add_standard_tables() const;

  bool create_table(const sharedptr<const TableInfo>& table_info, const Document_Glom::type_vecFields& fields) const;
  bool insert_example_data(const Glib::ustring& table_name) const;

  typedef std::vector< sharedptr<LayoutItem_Field> > type_vecLayoutFields;

protected:
  sharedptr<LayoutItem_Field> offer_field_list(const Glib::ustring& table_name, Gtk::Window* transient_for = 0);
  sharedptr<LayoutItem_Field> offer_field_list(const sharedptr<const LayoutItem_Field>& start_field, const Glib::ustring& table_name, Gtk::Window* transient_for = 0);
  sharedptr<LayoutItem_Field> offer_field_formatting(const sharedptr<const LayoutItem_Field>& start_field, const Glib::ustring& table_name, Gtk::Window* transient_for = 0);
  sharedptr<LayoutItem_Text> offer_textobject(const sharedptr<LayoutItem_Text>& start_textobject, Gtk::Window* transient_for = 0);
  sharedptr<LayoutItem_Notebook> offer_notebook(const sharedptr<LayoutItem_Notebook>& start_notebook, Gtk::Window* transient_for = 0);

  ///@result Whether the user would like to find again.
  static bool show_warning_no_records_found(Gtk::Window& transient_for);

  void fill_full_field_details(const Glib::ustring& parent_table_name, sharedptr<LayoutItem_Field>& layout_item);

  typedef std::vector<Glib::ustring> type_vecStrings;
  type_vecStrings get_table_names(bool ignore_system_tables = false) const;

  bool get_table_exists_in_database(const Glib::ustring& table_name) const;

  type_vecFields get_fields_for_table(const Glib::ustring& table_name) const;
  sharedptr<Field> get_fields_for_table_one_field(const Glib::ustring& table_name, const Glib::ustring& field_name) const;

  sharedptr<Field> get_field_primary_key_for_table(const Glib::ustring& table_name) const;

  Glib::ustring get_find_where_clause_quick(const Glib::ustring& table_name, const Gnome::Gda::Value& quick_search) const;


  virtual void set_entered_field_data(const sharedptr<const LayoutItem_Field>& field, const Gnome::Gda::Value&  value);
  virtual void set_entered_field_data(const Gtk::TreeModel::iterator& row, const sharedptr<const LayoutItem_Field>& field, const Gnome::Gda::Value& value);


  class FieldInRecord
  {
  public:
    FieldInRecord()
    {}

    FieldInRecord(const Glib::ustring& table_name, const sharedptr<const Field>& field, const sharedptr<const Field>& key, const Gnome::Gda::Value& key_value)
    : m_table_name(table_name), m_field(field), m_key(key), m_key_value(key_value)
    {
    }

    FieldInRecord(const sharedptr<const LayoutItem_Field>& layout_item, const Glib::ustring& parent_table_name, const sharedptr<const Field>& parent_key, const Gnome::Gda::Value& key_value, const Document_Glom& document)
    : m_key_value(key_value)
    {
      m_field = layout_item->get_full_field_details();
      m_table_name = layout_item->get_table_used(parent_table_name);

      //The key:
      if(layout_item->get_has_relationship_name())
      {
        //The field is in a related table.
        sharedptr<const Relationship> rel = layout_item->get_relationship();
        if(rel)
        {
          if(layout_item->get_has_related_relationship_name()) //For doubly-related fields
          {
            sharedptr<const Relationship> rel = layout_item->get_related_relationship();
            if(rel)
            {
              //Actually a foreign key in a doubly-related table:
              m_key = document.get_field(m_table_name, rel->get_to_field());
            }
          }
          else
          {
            //Actually a foreign key:
            m_key = document.get_field(m_table_name, rel->get_to_field());
          }
        }
      }
      else
      {
        m_key = parent_key;
      }
    }

    //Identify the field:
    Glib::ustring m_table_name;
    sharedptr<const Field> m_field;

    //Identify the record:
    sharedptr<const Field> m_key;
    Gnome::Gda::Value m_key_value;
  };

  /** Calculate values for fields, set them in the database, and show them in the layout.
   * @param field_changed The field that has changed, causing other fields to be recalculated because they use its value.
   * @param primary_key The primary key field for this table.
   * @param priamry_key_value: The primary key value for this record.
   * @param first_calc_field: false if this is called recursively.
   */
  void do_calculations(const FieldInRecord& field_changed, bool first_calc_field);

  typedef std::map<Glib::ustring, CalcInProgress> type_field_calcs;

  /** Get the fields whose values should be recalculated when @a field_name changes.
   */
  type_field_calcs get_calculated_fields(const Glib::ustring& table_name, const Glib::ustring& field_name);

  typedef std::list< sharedptr<LayoutItem_Field> > type_list_field_items;

  /** Get the fields used, if any, in the calculation of this field.
   */
  type_list_field_items get_calculation_fields(const Glib::ustring& table_name, const sharedptr<const Field>& field);

  void calculate_field(const FieldInRecord& field_in_record);

  void calculate_field_in_all_records(const Glib::ustring& table_name, const sharedptr<const Field>& field);
  void calculate_field_in_all_records(const Glib::ustring& table_name, const sharedptr<const Field>& field, const sharedptr<const Field>& primary_key);

  typedef std::map<Glib::ustring, Gnome::Gda::Value> type_map_fields;
  //TODO: Performance: This is massively inefficient:
  type_map_fields get_record_field_values(const Glib::ustring& table_name, const sharedptr<const Field> primary_key, const Gnome::Gda::Value& primary_key_value);


  void do_lookups(const FieldInRecord& field_in_record, const Gtk::TreeModel::iterator& row, const Gnome::Gda::Value& field_value);

  typedef std::pair< sharedptr<LayoutItem_Field>, sharedptr<Relationship> > type_pairFieldTrigger;
  typedef std::list<type_pairFieldTrigger> type_list_lookups;

  /** Get the fields whose values should be looked up when @a field_name changes, with
   * the relationship used to lookup the value.
   */
  type_list_lookups get_lookup_fields(const Glib::ustring& table_name, const Glib::ustring& field_name) const;


  /** Get the value of the @a source_field from the @a relationship, using the @a key_value.
   */
  Gnome::Gda::Value get_lookup_value(const Glib::ustring& table_name, const sharedptr<const Relationship>& relationship, const sharedptr<const Field>& source_field, const Gnome::Gda::Value & key_value);


  virtual void refresh_related_fields(const FieldInRecord& field_in_record_changed, const Gtk::TreeModel::iterator& row, const Gnome::Gda::Value& field_value);



  bool set_field_value_in_database(const FieldInRecord& field_in_record, const Gnome::Gda::Value& field_value, bool use_current_calculations = false);
  bool set_field_value_in_database(const FieldInRecord& field_in_record, const Gtk::TreeModel::iterator& row, const Gnome::Gda::Value& field_value, bool use_current_calculations = false);


  type_vecStrings get_database_groups() const;
  type_vecStrings get_database_users(const Glib::ustring& group_name = Glib::ustring()) const;
  Privileges get_table_privileges(const Glib::ustring& group_name, const Glib::ustring& table_name) const;
  void set_table_privileges(const Glib::ustring& group_name, const Glib::ustring& table_name, const Privileges& privs, bool developer_privs = false);
  Glib::ustring get_user_visible_group_name(const Glib::ustring& group_name) const;

  type_vecStrings get_groups_of_user(const Glib::ustring& user) const;
  bool get_user_is_in_group(const Glib::ustring& user, const Glib::ustring& group) const;
  Privileges get_current_privs(const Glib::ustring& table_name) const;

  SystemPrefs get_database_preferences() const;
  void set_database_preferences(const SystemPrefs& prefs);

  void report_build(const Glib::ustring& table_name, const sharedptr<const Report>& report, const Glib::ustring& where_clause, Gtk::Window* parent_window = 0);

  void report_build_groupby(const Glib::ustring& table_name, xmlpp::Element& parent_node, const sharedptr<LayoutItem_GroupBy>& group_by, const Glib::ustring& where_clause_parent);
  void report_build_groupby_children(const Glib::ustring& table_name, xmlpp::Element& nodeGroupBy, const sharedptr<LayoutItem_GroupBy>& group_by, const Glib::ustring& where_clause);
  void report_build_summary(const Glib::ustring& table_name, xmlpp::Element& parent_node, const sharedptr<LayoutItem_Summary>& summary, const Glib::ustring& where_clause_parent);

  typedef std::vector< sharedptr<LayoutItem> > type_vecLayoutItems;

  void report_build_records(const Glib::ustring& table_name, xmlpp::Element& parent_node, const type_vecLayoutItems& items, const Glib::ustring& where_clause, const Glib::ustring& sort_clause = Glib::ustring(), bool one_record_only = false);
  void report_build_records_get_fields(const Glib::ustring& table_name, const sharedptr<LayoutGroup>& group, type_vecLayoutFields& items);
  void report_build_records_field(const Glib::ustring& table_name, xmlpp::Element& nodeParent, const sharedptr<const LayoutItem_Field>& field, const Glib::RefPtr<Gnome::Gda::DataModel>& datamodel, guint row, guint& colField, bool vertical = false);
  void report_build_records_text(const Glib::ustring& table_name, xmlpp::Element& nodeParent, const sharedptr<const LayoutItem_Text>& textobject);
  void report_build_records_vertical_group(const Glib::ustring& table_name, xmlpp::Element& vertical_group_node, const sharedptr<LayoutItem_VerticalGroup>& group, const Glib::RefPtr<Gnome::Gda::DataModel>& datamodel, guint row, guint& field_index);


  Gnome::Gda::Value auto_increment_insert_first_if_necessary(const Glib::ustring& table_name, const Glib::ustring& field_name) const;

  /** Get the next auto-increment value for this primary key, from the glom system table.
   * Add a row for this field in the system table if it does not exist already.
   */
  Gnome::Gda::Value get_next_auto_increment_value(const Glib::ustring& table_name, const Glib::ustring& field_name) const;

  /** Set the next auto-increment value in the glom system table, by examining all current values.
   * Use this, for instance, after importing rows.
   * Add a row for this field in the system table if it does not exist already.
   */
  void recalculate_next_auto_increment_value(const Glib::ustring& table_name, const Glib::ustring& field_name) const;

  virtual bool fill_from_database();
  virtual void fill_end(); //Call this from the end of fill_from_database() overrides.

  virtual void on_userlevel_changed(AppState::userlevels userlevel);

  type_vecLayoutFields get_table_fields_to_show_for_sequence(const Glib::ustring& table_name, const Document_Glom::type_mapLayoutGroupSequence& mapGroupSequence) const;
  void get_table_fields_to_show_for_sequence_add_group(const Glib::ustring& table_name, const Privileges& table_privs, const type_vecFields& all_db_fields, const sharedptr<const LayoutGroup>& group, type_vecLayoutFields& vecFields) const;

  static bool get_field_primary_key_index_for_fields(const type_vecFields& fields, guint& field_column);
  static bool get_field_primary_key_index_for_fields(const type_vecLayoutFields& fields, guint& field_column);

  static Glib::ustring util_string_from_decimal(guint decimal);
  static guint util_decimal_from_string(const Glib::ustring& str);

  static bool util_string_has_whitespace(const Glib::ustring& text);

  static type_vecStrings util_vecStrings_from_Fields(const type_vecFields& fields);

  //Utlility functions to help with the odd formats of postgres internal catalog fields:
  static Glib::ustring string_trim(const Glib::ustring& str, const Glib::ustring& to_remove);
  static type_vecStrings string_separate(const Glib::ustring& str, const Glib::ustring& separator);
  static type_vecStrings pg_list_separate(const Glib::ustring& str);


  void handle_error(const std::exception& ex) const; //TODO_port: This is probably useless now.
  bool handle_error() const;

  type_field_calcs m_FieldsCalculationInProgress; //Prevent circular calculations and recalculations.
};

#endif //BASE_DB_H
