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

#include "config.h" // For GLOM_ENABLE_CLIENT_ONLY

#include "box_data.h"
#include <libglom/data_structure/glomconversions.h>
#include <glom/utils_ui.h>
#include <libglom/data_structure/layout/layoutitem_field.h>
#include <libglom/db_utils.h>
#include <libglom/privs.h>
#include <glom/python_embed/glom_python.h>
#include <glom/python_embed/python_ui_callbacks.h>
#include <glom/appwindow.h>
#include <algorithm> //For std::find()
#include <libglom/libglom_config.h>
#include <iostream>
#include <glibmm/i18n.h>


namespace Glom
{

Box_Data::Box_Data()
: m_Button_Find(_("_Find"), true)
#ifndef GLOM_ENABLE_CLIENT_ONLY
  ,m_pDialogLayout(0)
#endif // !GLOM_ENABLE_CLIENT_ONLY
{
  m_bUnstoredData = false;

  //Connect signals:
  m_Button_Find.signal_clicked().connect(sigc::mem_fun(*this, &Box_Data::on_Button_Find));
}

Box_Data::~Box_Data()
{
#ifndef GLOM_ENABLE_CLIENT_ONLY
  if(m_pDialogLayout)
  {
    remove_view(m_pDialogLayout);
    delete m_pDialogLayout;
  }
#endif // !GLOM_ENABLE_CLIENT_ONLY
}

bool Box_Data::init_db_details(const FoundSet& found_set, const Glib::ustring& layout_platform)
{
  m_layout_platform = layout_platform;
  m_table_name = found_set.m_table_name;
  m_found_set = found_set;

  create_layout(); //So that fill_from_database() can succeed.

  return Base_DB_Table_Data::init_db_details(m_table_name); //Calls fill_from_database().
}

bool Box_Data::refresh_data_from_database_with_where_clause(const FoundSet& found_set)
{
  m_found_set = found_set;

  return Base_DB_Table_Data::refresh_data_from_database(); //Calls fill_from_database().
}

FoundSet Box_Data::get_found_set() const
{
  return m_found_set;
}

Gnome::Gda::SqlExpr Box_Data::get_find_where_clause() const
{
  Glib::RefPtr<Gnome::Gda::SqlBuilder> builder =
    Gnome::Gda::SqlBuilder::create(Gnome::Gda::SQL_STATEMENT_SELECT);
  builder->select_add_target(m_table_name);  //This might not be necessary.
  guint where_cond_id = 0;

  Glib::RefPtr<Gnome::Gda::Connection> connection = get_connection();
  if(!connection)
  {
    std::cerr << G_STRFUNC << ": connection was null." << std::endl;
    return Gnome::Gda::SqlExpr();
  }

  //Look at each field entry and build e.g. 'Name = "Bob"'
  for(type_vecConstLayoutFields::const_iterator iter = m_FieldsShown.begin(); iter != m_FieldsShown.end(); ++iter)
  {
    const Gnome::Gda::Value data = get_entered_field_data(*iter);

    if(!Conversions::value_is_empty(data))
    {
      const sharedptr<const Field> field = (*iter)->get_full_field_details();
      if(field)
      {
        bool use_this_field = true;
        if(field->get_glom_type() == Field::TYPE_BOOLEAN) //TODO: We need an intermediate state for boolean fields, so that they can be ignored in searches.
        {
          if(!data.get_boolean())
            use_this_field = false;
        }

        if(use_this_field)
        {
          const guint cond_id = builder->add_cond(field->sql_find_operator(),
            builder->add_field_id(field->get_name(), m_table_name),
            builder->add_expr( field->sql_find(data, connection) ));

          //And with previous condition, if any:
          if(where_cond_id)
          {
            where_cond_id = builder->add_cond(Gnome::Gda::SQL_OPERATOR_TYPE_AND,
              where_cond_id,
              cond_id);
          }
          else
            where_cond_id = cond_id;
        }
      }
    }
  }

  if(where_cond_id)
  {
    builder->set_where(where_cond_id); //This might not be necessary.
    return builder->export_expression(where_cond_id);
  }
  else
    return Gnome::Gda::SqlExpr();
}

void Box_Data::on_Button_Find()
{
  //Make sure that the cell is updated:
  //m_AddDel.finish_editing();

  //Call the virtual method to get the find criteria for a details or list view:
  const Gnome::Gda::SqlExpr where_clause = get_find_where_clause();

  //The signal handler then checks and warns if no find criteria were entered.
  signal_find_criteria.emit(where_clause);
}

void Box_Data::set_unstored_data(bool bVal)
{
  m_bUnstoredData = bVal;
}

bool Box_Data::get_unstored_data() const
{
  return m_bUnstoredData;
}

void Box_Data::create_layout()
{
  set_unstored_data(false);

  //Cache the table information, for performance:
  const Document* document = dynamic_cast<const Document*>(get_document());
  m_TableFields = DbUtils::get_fields_for_table(document, m_table_name);
}

bool Box_Data::fill_from_database()
{
  set_unstored_data(false);

  return Base_DB_Table_Data::fill_from_database();
}

bool Box_Data::confirm_discard_unstored_data() const
{
  if(get_unstored_data())
  {
    const Glib::ustring message = _("This data cannot be stored in the database because you have not provided a primary key.\nDo you really want to discard this data?");
    //Ask user to confirm loss of data:
    Gtk::MessageDialog dialog(UiUtils::bold_message(_("No primary key value")), true, Gtk::MESSAGE_QUESTION, Gtk::BUTTONS_OK_CANCEL );
    dialog.set_secondary_text(message);
    //TODO: It needs a const. I wonder if it should. murrayc. dialog.set_transient_for(*get_app_window());
    const int iButton = dialog.run();

    return (iButton == Gtk::RESPONSE_OK);
  }
  else
  {
    return true; //no data to lose.
  }
}

#ifndef GLOM_ENABLE_CLIENT_ONLY
void Box_Data::show_layout_dialog()
{
  if(!m_pDialogLayout)
  {
    m_pDialogLayout = create_layout_dialog();
    add_view(m_pDialogLayout); //Give it access to the document.
    m_pDialogLayout->signal_hide().connect( sigc::mem_fun(*this, &Box_Data::on_dialog_layout_hide) );
  }

  prepare_layout_dialog(m_pDialogLayout);
  m_pDialogLayout->show();
}
#endif // !GLOM_ENABLE_CLIENT_ONLY

#ifndef GLOM_ENABLE_CLIENT_ONLY
void Box_Data::on_dialog_layout_hide()
{
  //Re-fill view, in case the layout has changed:
  create_layout();

  if(ConnectionPool::get_instance()->get_ready_to_connect())
    fill_from_database();
}
#endif // !GLOM_ENABLE_CLIENT_ONLY

Box_Data::type_vecConstLayoutFields Box_Data::get_fields_to_show() const
{
  if(m_table_name.empty())
  {
    return type_vecConstLayoutFields();
  }
  else
    return get_table_fields_to_show(m_table_name);
}

Box_Data::type_vecConstLayoutFields Box_Data::get_table_fields_to_show(const Glib::ustring& table_name) const
{
  const Document* pDoc = dynamic_cast<const Document*>(get_document());
  if(pDoc)
  {
    Document::type_list_layout_groups mapGroupSequence = pDoc->get_data_layout_groups_plus_new_fields(m_layout_name, table_name, m_layout_platform);
    return get_table_fields_to_show_for_sequence(table_name, mapGroupSequence);
  }
  else
    return type_vecConstLayoutFields();
}

Document::type_list_layout_groups Box_Data::get_data_layout_groups(const Glib::ustring& layout_name, const Glib::ustring& layout_platform)
{
  Document::type_list_layout_groups layout_groups;

  Document* document = dynamic_cast<Document*>(get_document());
  if(document)
  {
    if(!m_table_name.empty())
    {
      //Get the layout information from the document:
      layout_groups = document->get_data_layout_groups_plus_new_fields(layout_name, m_table_name, layout_platform);
      document->fill_layout_field_details(m_table_name, layout_groups); //TODO: Do this automatically in Document?
      const Privileges table_privs = Privs::get_current_privs(m_table_name);

      //Fill in the field information for the fields mentioned in the layout:
      for(Document::type_list_layout_groups::iterator iterGroups = layout_groups.begin(); iterGroups != layout_groups.end(); ++iterGroups)
      {
        fill_layout_group_field_info(*iterGroups, table_privs);

        //std::cout << "debug: Box_Data::get_data_layout_groups: " << std::endl;
        //*iterGroups->debug();
      }
    }
  }

  return layout_groups;
}

void Box_Data::fill_layout_group_field_info(const sharedptr<LayoutGroup>& group, const Privileges& table_privs)
{
  if(!group)
   return;

  const Document* document = get_document();

  LayoutGroup::type_list_items items = group->get_items();
  for(LayoutGroup::type_list_items::iterator iter = items.begin(); iter != items.end(); ++iter)
  {
    sharedptr<LayoutItem> item = *iter;
    sharedptr<LayoutItem_Field> item_field = sharedptr<LayoutItem_Field>::cast_dynamic(item);
    if(item_field) //If is a field rather than some other layout item
    {

      if(item_field->get_has_relationship_name()) //If it's a field in a related table.
      {
        //Get the full field information:
        const Glib::ustring relationship_name = item_field->get_relationship_name();
        sharedptr<const Relationship> relationship = document->get_relationship(m_table_name, relationship_name);
        if(relationship)
        {
          sharedptr<Field> field = DbUtils::get_fields_for_table_one_field(document, relationship->get_to_table(), item->get_name());
          if(field)
          {
            item_field->set_full_field_details(field);

            //TODO_Performance: Don't do this repeatedly for the same table.
            const Privileges privs = Privs::get_current_privs(relationship->get_to_table());
            item_field->m_priv_view = privs.m_view;
            item_field->m_priv_edit = privs.m_edit;
          }
        }
      }
      else
      {
        //Get the field info:
        sharedptr<Field> field = DbUtils::get_fields_for_table_one_field(document, m_table_name, item_field->get_name());
        if(field)
        {
          item_field->set_full_field_details(field); //TODO_Performance: Just use this as the output arg?
          item_field->m_priv_view = table_privs.m_view;
          item_field->m_priv_edit = table_privs.m_edit;
        }
      }
    }
    else
    {
      sharedptr<LayoutGroup> item_group = sharedptr<LayoutGroup>::cast_dynamic(item);
      if(item_group) //If it is a group
      {
        //recurse, to fill the fields info in this group:
        fill_layout_group_field_info(item_group, table_privs);
      }
    }
  }
}

void Box_Data::print_layout()
{
  const Glib::ustring message = "Sorry, this feature has not been implemented yet.";
  Gtk::MessageDialog dialog("<b>Not implemented</b>", true);
  dialog.set_secondary_text(message);
  dialog.set_transient_for(*get_app_window());
  dialog.run();
}

Glib::ustring Box_Data::get_layout_name() const
{
  return m_layout_name;
}

void Box_Data::execute_button_script(const sharedptr<const LayoutItem_Button>& layout_item, const Gnome::Gda::Value& primary_key_value)
{
  const Glib::ustring script = layout_item->get_script();
  if(!UiUtils::script_check_for_pygtk2_with_warning(script, get_app_window()))
    return;

  const sharedptr<Field> field_primary_key = get_field_primary_key();
  const type_map_fields field_values = get_record_field_values_for_calculation(m_table_name, field_primary_key, primary_key_value);

  //We need the connection when we run the script, so that the script may use it.
  sharedptr<SharedConnection> sharedconnection = connect_to_server(0 /* parent window */);

  //Allow this UI to respond to UI change requests from the Python code:
  AppPythonUICallbacks callbacks;

  Glib::ustring error_message;
  glom_execute_python_function_implementation(script,
    field_values, //TODO: Maybe use the field's type here.
    get_document(),
    get_table_name(), field_primary_key, primary_key_value,
    sharedconnection->get_gda_connection(),
    callbacks,
    error_message);
  if(!error_message.empty())
  {
    std::cerr << G_STRFUNC << ": Python Error: " << error_message << std::endl;
  }
}

} //namespace Glom
