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

#include "dialog_sortfields.h"
#include "dialog_field_layout.h"

//#include <libgnome/gnome-i18n.h>
#include <glibmm/i18n.h>

namespace Glom
{

const char* Dialog_SortFields::glade_id("dialog_sort_fields");
const bool Dialog_SortFields::glade_developer(true);

Dialog_SortFields::Dialog_SortFields(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder)
: Dialog_Layout(cobject, builder, false /* means no table title */),
  m_treeview_fields(0),
  m_button_field_up(0),
  m_button_field_down(0),
  m_button_field_add(0),
  m_button_field_delete(0),
  m_button_field_edit(0),
  m_label_table_name(0)
{
  builder->get_widget("label_table_name", m_label_table_name);

  builder->get_widget("treeview_fields", m_treeview_fields);
  if(m_treeview_fields)
  {
    m_model_fields = Gtk::ListStore::create(m_ColumnsFields);
    m_treeview_fields->set_model(m_model_fields);

    // Append the View columns:
    Gtk::TreeView::Column* column_name = Gtk::manage( new Gtk::TreeView::Column(_("Name")) );
    m_treeview_fields->append_column(*column_name);

    Gtk::CellRendererText* renderer_name = Gtk::manage(new Gtk::CellRendererText);
    column_name->pack_start(*renderer_name);
    column_name->set_cell_data_func(*renderer_name, sigc::mem_fun(*this, &Dialog_SortFields::on_cell_data_name));

    m_treeview_fields->append_column_editable(_("Ascending"), m_ColumnsFields.m_col_ascending);

    //Sort by sequence, so we can change the order by changing the values in the hidden sequence column.
    m_model_fields->set_sort_column(m_ColumnsFields.m_col_sequence, Gtk::SORT_ASCENDING);


    //Respond to changes of selection:
    Glib::RefPtr<Gtk::TreeView::Selection> refSelection = m_treeview_fields->get_selection();
    if(refSelection)
    {
      refSelection->signal_changed().connect( sigc::mem_fun(*this, &Dialog_SortFields::on_treeview_fields_selection_changed) );
    }

    m_model_fields->signal_row_changed().connect( sigc::mem_fun(*this, &Dialog_SortFields::on_treemodel_row_changed) );
  }


  builder->get_widget("button_field_up", m_button_field_up);
  m_button_field_up->signal_clicked().connect( sigc::mem_fun(*this, &Dialog_SortFields::on_button_field_up) );

  builder->get_widget("button_field_down", m_button_field_down);
  m_button_field_down->signal_clicked().connect( sigc::mem_fun(*this, &Dialog_SortFields::on_button_field_down) );

  builder->get_widget("button_field_delete", m_button_field_delete);
  m_button_field_delete->signal_clicked().connect( sigc::mem_fun(*this, &Dialog_SortFields::on_button_delete) );

  builder->get_widget("button_field_add", m_button_field_add);
  m_button_field_add->signal_clicked().connect( sigc::mem_fun(*this, &Dialog_SortFields::on_button_add_field) );

  builder->get_widget("button_field_edit", m_button_field_edit);
  m_button_field_edit->signal_clicked().connect( sigc::mem_fun(*this, &Dialog_SortFields::on_button_edit_field) );

  show_all_children();
}

Dialog_SortFields::~Dialog_SortFields()
{
}

void Dialog_SortFields::set_fields(const Glib::ustring& table_name, const LayoutItem_GroupBy::type_list_sort_fields& fields)
{
  m_modified = false;
  m_table_name = table_name;

  Document* document = get_document();

  //Update the tree models from the document
  if(document)
  {
    //Set the table name and title:
    m_label_table_name->set_text(table_name);


    //Show the field layout
    m_model_fields->clear();
    guint field_sequence = 0;
    for(LayoutItem_GroupBy::type_list_sort_fields::const_iterator iter = fields.begin(); iter != fields.end(); ++iter)
    {
      sharedptr<const LayoutItem_Field> item = sharedptr<const LayoutItem_Field>::cast_dynamic(iter->first);

      Gtk::TreeModel::iterator iterTree = m_model_fields->append();
      Gtk::TreeModel::Row row = *iterTree;

      row[m_ColumnsFields.m_col_layout_item] = item;
      row[m_ColumnsFields.m_col_ascending] = iter->second;
      row[m_ColumnsFields.m_col_sequence] = field_sequence;
      ++field_sequence;
    }

    //treeview_fill_sequences(m_model_fields, m_ColumnsFields.m_col_sequence); //The document should have checked this already, but it does not hurt to check again.
  }

  m_modified = false;
}

void Dialog_SortFields::enable_buttons()
{
  //Fields:
  Glib::RefPtr<Gtk::TreeView::Selection> refSelection = m_treeview_fields->get_selection();
  if(refSelection)
  {
    Gtk::TreeModel::iterator iter = refSelection->get_selected();
    if(iter)
    {
      //Disable Up if It can't go any higher.
      bool enable_up = true;
      if(iter == m_model_fields->children().begin())
        enable_up = false;  //It can't go any higher.

      m_button_field_up->set_sensitive(enable_up);


      //Disable Down if It can't go any lower.
      Gtk::TreeModel::iterator iterNext = iter;
      iterNext++;

      bool enable_down = true;
      if(iterNext == m_model_fields->children().end())
        enable_down = false;

      m_button_field_down->set_sensitive(enable_down);

      m_button_field_delete->set_sensitive(true);
    }
    else
    {
      //Disable all buttons that act on a selection:
      m_button_field_down->set_sensitive(false);
      m_button_field_up->set_sensitive(false);
      m_button_field_delete->set_sensitive(false);
    }
  }


}


void Dialog_SortFields::on_button_field_up()
{
  move_treeview_selection_up(m_treeview_fields, m_ColumnsFields.m_col_sequence);
}

void Dialog_SortFields::on_button_field_down()
{
  move_treeview_selection_down(m_treeview_fields, m_ColumnsFields.m_col_sequence);
}

LayoutItem_GroupBy::type_list_sort_fields Dialog_SortFields::get_fields() const
{
  LayoutItem_GroupBy::type_list_sort_fields result;

  guint field_sequence = 1; //0 means no sequence
  for(Gtk::TreeModel::iterator iterFields = m_model_fields->children().begin(); iterFields != m_model_fields->children().end(); ++iterFields)
  {
    Gtk::TreeModel::Row row = *iterFields;

    sharedptr<const LayoutItem_Field> item = row[m_ColumnsFields.m_col_layout_item];
    const Glib::ustring field_name = item->get_name();
    if(!field_name.empty())
    {
      sharedptr<LayoutItem_Field> field_copy = glom_sharedptr_clone(item);

      const bool ascending = row[m_ColumnsFields.m_col_ascending];
      result.push_back( LayoutItem_GroupBy::type_pair_sort_field(field_copy, ascending) );

      ++field_sequence;
    }
  }

  return result;
}

void Dialog_SortFields::on_treeview_fields_selection_changed()
{
  enable_buttons();
}

void Dialog_SortFields::on_button_add_field()
{
  //Get the chosen fields:
  type_list_field_items fields_list = offer_field_list(m_table_name, this);
  for(type_list_field_items::iterator iter_chosen = fields_list.begin(); iter_chosen != fields_list.end(); ++iter_chosen) 
  {

    sharedptr<LayoutItem_Field> field = *iter_chosen;
    if(!field)
      continue;

    //Add the field details to the layout treeview:
    Gtk::TreeModel::iterator iter =  m_model_fields->append();

    if(iter)
    {
      Gtk::TreeModel::Row row = *iter;
      row[m_ColumnsFields.m_col_layout_item] = field;
      row[m_ColumnsFields.m_col_ascending] = true; //Default to this so that alphabetical searches go from A to Z by default.

      //Scroll to, and select, the new row:
      Glib::RefPtr<Gtk::TreeView::Selection> refTreeSelection = m_treeview_fields->get_selection();
      if(refTreeSelection)
        refTreeSelection->select(iter);

      m_treeview_fields->scroll_to_row( Gtk::TreeModel::Path(iter) );

      treeview_fill_sequences(m_model_fields, m_ColumnsFields.m_col_sequence); //The document should have checked this already, but it does not hurt to check again.
    }
  }
}

void Dialog_SortFields::on_button_delete()
{
  Glib::RefPtr<Gtk::TreeView::Selection> refTreeSelection = m_treeview_fields->get_selection();
  if(refTreeSelection)
  {
    //TODO: Handle multiple-selection:
    Gtk::TreeModel::iterator iter = refTreeSelection->get_selected();
    if(iter)
    {
      m_model_fields->erase(iter);

      m_modified = true;
    }
  }
}


void Dialog_SortFields::on_cell_data_name(Gtk::CellRenderer* renderer, const Gtk::TreeModel::iterator& iter)
{
  //Set the view's cell properties depending on the model's data:
  Gtk::CellRendererText* renderer_text = dynamic_cast<Gtk::CellRendererText*>(renderer);
  if(renderer_text)
  {
    if(iter)
    {
      Gtk::TreeModel::Row row = *iter;

      sharedptr<const LayoutItem_Field> item = row[m_ColumnsFields.m_col_layout_item]; //TODO_performance: Reduce copying.
      renderer_text->property_markup() = item->get_layout_display_name();
      renderer_text->property_editable() = false; //Names can never be edited.
    }
  }
}


void Dialog_SortFields::on_button_edit_field()
{
  Glib::RefPtr<Gtk::TreeView::Selection> refTreeSelection = m_treeview_fields->get_selection();
  if(refTreeSelection)
  {
    //TODO: Handle multiple-selection:
    Gtk::TreeModel::iterator iter = refTreeSelection->get_selected();
    if(iter)
    {
      Gtk::TreeModel::Row row = *iter;
      sharedptr<const LayoutItem_Field> field = row[m_ColumnsFields.m_col_layout_item];

      //Get the chosen field:
      sharedptr<LayoutItem_Field> field_chosen = 
        offer_field_list_select_one_field(field, m_table_name, this);
      if(field_chosen)

      //Set the field details in the layout treeview:

      row[m_ColumnsFields.m_col_layout_item] = field_chosen;

      //Scroll to, and select, the new row:
      /*
      Glib::RefPtr<Gtk::TreeView::Selection> refTreeSelection = m_treeview_fields->get_selection();
      if(refTreeSelection)
        refTreeSelection->select(iter);

      m_treeview_fields->scroll_to_row( Gtk::TreeModel::Path(iter) );

      treeview_fill_sequences(m_model_fields, m_ColumnsFields.m_col_sequence); //The document should have checked this already, but it does not hurt to check again.
      */
    }
  }
}

} //namespace Glom


