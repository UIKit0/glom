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

#include <glom/mode_design/layout/dialog_layout_export.h>
#include <glom/mode_design/layout/dialog_choose_field.h>
#include <glom/mode_design/layout/layout_item_dialogs/dialog_field_layout.h>
#include <libglom/utils.h> //For bold_message()).

//#include <libgnome/gnome-i18n.h>
#include <glibmm/i18n.h>

namespace Glom
{

Dialog_Layout_Export::Dialog_Layout_Export(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& builder)
: Dialog_Layout(cobject, builder, false /* no table title */),
  m_treeview_fields(0),
  m_button_field_up(0),
  m_button_field_down(0),
  m_button_field_add(0),
  m_button_field_delete(0),
  m_button_field_edit(0),
  m_label_table_name(0)
{
  builder->get_widget("label_table_name", m_label_table_name);
  m_entry_table_title = 0; //Not in this glade file.

  builder->get_widget("treeview_fields", m_treeview_fields);
  if(m_treeview_fields)
  {
    m_model_fields = Gtk::ListStore::create(m_ColumnsFields);
    m_treeview_fields->set_model(m_model_fields);

    // Append the View columns:
    Gtk::TreeView::Column* column_name = Gtk::manage( new Gtk::TreeView::Column(_("Fields")) );
    m_treeview_fields->append_column(*column_name);

    Gtk::CellRendererText* renderer_name = Gtk::manage(new Gtk::CellRendererText);
    column_name->pack_start(*renderer_name);
    column_name->set_cell_data_func(*renderer_name, sigc::mem_fun(*this, &Dialog_Layout_Export::on_cell_data_name));


    //Sort by sequence, so we can change the order by changing the values in the hidden sequence column.
    m_model_fields->set_sort_column(m_ColumnsFields.m_col_sequence, Gtk::SORT_ASCENDING);


    //Respond to changes of selection:
    Glib::RefPtr<Gtk::TreeView::Selection> refSelection = m_treeview_fields->get_selection();
    if(refSelection)
    {
      refSelection->signal_changed().connect( sigc::mem_fun(*this, &Dialog_Layout_Export::on_treeview_fields_selection_changed) );
    }

    m_model_fields->signal_row_changed().connect( sigc::mem_fun(*this, &Dialog_Layout_Export::on_treemodel_row_changed) );
  }


  builder->get_widget("button_field_up", m_button_field_up);
  m_button_field_up->signal_clicked().connect( sigc::mem_fun(*this, &Dialog_Layout_Export::on_button_up) );

  builder->get_widget("button_field_down", m_button_field_down);
  m_button_field_down->signal_clicked().connect( sigc::mem_fun(*this, &Dialog_Layout_Export::on_button_down) );

  builder->get_widget("button_field_delete", m_button_field_delete);
  m_button_field_delete->signal_clicked().connect( sigc::mem_fun(*this, &Dialog_Layout_Export::on_button_delete) );

  builder->get_widget("button_field_add", m_button_field_add);
  m_button_field_add->signal_clicked().connect( sigc::mem_fun(*this, &Dialog_Layout_Export::on_button_add_field) );

  builder->get_widget("button_field_edit", m_button_field_edit);
  m_button_field_edit->signal_clicked().connect( sigc::mem_fun(*this, &Dialog_Layout_Export::on_button_edit_field) );

  show_all_children();
}

Dialog_Layout_Export::~Dialog_Layout_Export()
{
}

void Dialog_Layout_Export::set_layout_groups(Document::type_list_layout_groups& mapGroups, Document* document, const Glib::ustring& table_name)
{
  Base_DB::set_document(document);

  m_modified = false;
  //m_document = document

  m_table_name = table_name;

  //Update the tree models from the document
  if(document)
  {
    //Set the table name and title:
    m_label_table_name->set_text(table_name);

    if(document)
      document->fill_layout_field_details(m_table_name, mapGroups); //Update with full field information.

    //If we have not layout information, then start with something:
    /*
    if(mapGroups.empty())
    {
      LayoutGroup group;
      group->set_name("main");

      guint field_sequence = 1; //0 means no sequence
      for(type_vecLayoutFields::const_iterator iter = table_fields.begin(); iter != table_fields.end(); ++iter)
      {
        LayoutItem_Field item = *(*iter);
        group->add_item(item);

        ++field_sequence;
      }

      mapGroups[1] = group;
    }
    */

    //Show the field layout
    typedef std::list< Glib::ustring > type_listStrings;

    m_model_fields->clear();

    guint field_sequence = 1; //0 means no sequence
    for(Document::type_list_layout_groups::const_iterator iter = mapGroups.begin(); iter != mapGroups.end(); ++iter)
    {
      sharedptr<const LayoutGroup> group = *iter;
      if(!group)
        continue;

      //Add the group's fields:
      LayoutGroup::type_list_const_items items = group->get_items();
      for(LayoutGroup::type_list_const_items::const_iterator iter = items.begin(); iter != items.end(); ++iter)
      {
        sharedptr<const LayoutItem_Field> item = sharedptr<const LayoutItem_Field>::cast_dynamic(*iter); 
        if(item)
        {
          Gtk::TreeModel::iterator iterTree = m_model_fields->append();
          Gtk::TreeModel::Row row = *iterTree;

          row[m_ColumnsFields.m_col_layout_item] = glom_sharedptr_clone(item);
          row[m_ColumnsFields.m_col_sequence] = field_sequence;
          ++field_sequence;
        }
      }
    }

    treeview_fill_sequences(m_model_fields, m_ColumnsFields.m_col_sequence); //The document should have checked this already, but it does not hurt to check again.
  }

  m_modified = false;
}

void Dialog_Layout_Export::enable_buttons()
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


void Dialog_Layout_Export::on_button_up()
{
  move_treeview_selection_up(m_treeview_fields, m_ColumnsFields.m_col_sequence);
}

void Dialog_Layout_Export::on_button_down()
{
  move_treeview_selection_down(m_treeview_fields, m_ColumnsFields.m_col_sequence);
}

void Dialog_Layout_Export::get_layout_groups(Document::type_list_layout_groups& layout_groups) const
{
  //Get the data from the TreeView and store it in the document:

  //Get the groups and their fields:
  Document::type_list_layout_groups groups;

  //Add the fields to the one group:
  sharedptr<LayoutGroup> others = sharedptr<LayoutGroup>::create();
  others->set_name("main");

  guint field_sequence = 1; //0 means no sequence
  for(Gtk::TreeModel::iterator iterFields = m_model_fields->children().begin(); iterFields != m_model_fields->children().end(); ++iterFields)
  {
    Gtk::TreeModel::Row row = *iterFields;

    sharedptr<LayoutItem_Field> item = row[m_ColumnsFields.m_col_layout_item];
    const Glib::ustring field_name = item->get_name();
    if(!field_name.empty())
    {
      others->add_item(item); //Add it to the group:

      ++field_sequence;
    }
  }

  groups.push_back(others);
  layout_groups.swap(groups);
}

void Dialog_Layout_Export::on_treeview_fields_selection_changed()
{
  enable_buttons();
}

void Dialog_Layout_Export::on_button_add_field()
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

      //Scroll to, and select, the new row:
      Glib::RefPtr<Gtk::TreeView::Selection> refTreeSelection = m_treeview_fields->get_selection();
      if(refTreeSelection)
        refTreeSelection->select(iter);

      m_treeview_fields->scroll_to_row( Gtk::TreeModel::Path(iter) );

      treeview_fill_sequences(m_model_fields, m_ColumnsFields.m_col_sequence); //The document should have checked this already, but it does not hurt to check again.
    }
  }

  enable_buttons();
}

void Dialog_Layout_Export::on_button_delete()
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

  enable_buttons();
}


void Dialog_Layout_Export::on_cell_data_name(Gtk::CellRenderer* renderer, const Gtk::TreeModel::iterator& iter)
{
  //Set the view's cell properties depending on the model's data:
  Gtk::CellRendererText* renderer_text = dynamic_cast<Gtk::CellRendererText*>(renderer);
  if(renderer_text)
  {
    if(iter)
    {
      Gtk::TreeModel::Row row = *iter;

      //Indicate that it's a field in another table.
      sharedptr<LayoutItem_Field> item = row[m_ColumnsFields.m_col_layout_item];

      //Names can never be edited.
      g_object_set(renderer_text->gobj(), "markup", item->get_layout_display_name().c_str(), "editable", FALSE, (gpointer)0);
    }
  }
}


void Dialog_Layout_Export::on_button_edit_field()
{
  Glib::RefPtr<Gtk::TreeView::Selection> refTreeSelection = m_treeview_fields->get_selection();
  if(refTreeSelection)
  {
    //TODO: Handle multiple-selection:
    Gtk::TreeModel::iterator iter = refTreeSelection->get_selected();
    if(iter)
    {
      Gtk::TreeModel::Row row = *iter;
      sharedptr<LayoutItem_Field> field = row[m_ColumnsFields.m_col_layout_item];

      //Get the chosen field:
      sharedptr<LayoutItem_Field> field_chosen = offer_field_list_select_one_field(field, m_table_name, this);
      if(field_chosen)
      {
        //Set the field details in the layout treeview:

        row[m_ColumnsFields.m_col_layout_item] = field;

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
}

} //namespace Glom
