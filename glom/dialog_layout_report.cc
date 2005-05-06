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

#include "dialog_layout_report.h"
#include "data_structure/layout/layoutgroup.h"
#include "data_structure/layout/layoutitem_field.h"
#include "mode_data/dialog_choose_field.h"
#include "mode_data/dialog_field_layout.h"
#include "mode_data/dialog_choose_relationship.h"
//#include <libgnome/gnome-i18n.h>
#include <bakery/App/App_Gtk.h> //For util_bold_message().
#include <glibmm/i18n.h>
#include <sstream> //For stringstream

Dialog_Layout_Report::Dialog_Layout_Report(BaseObjectType* cobject, const Glib::RefPtr<Gnome::Glade::Xml>& refGlade)
: Dialog_Layout(cobject, refGlade, false /* No table title */),
  m_treeview_parts(0),
  m_treeview_available_parts(0),
  m_button_up(0),
  m_button_down(0),
  m_button_add(0),
  m_button_delete(0),
  m_button_edit(0),
  m_label_table_name(0),
  m_entry_name(0),
  m_entry_title(0)
{
  refGlade->get_widget("label_table_name", m_label_table_name);
  refGlade->get_widget("entry_name", m_entry_name);
  refGlade->get_widget("entry_title", m_entry_title);

  //Available parts:
  refGlade->get_widget("treeview_available_parts", m_treeview_available_parts);
  if(m_treeview_available_parts)
  {
    //Add list of available parts:
    //These are deleted in the destructor:
    m_model_available_parts = Gtk::TreeStore::create(m_columns_available_parts);

    Gtk::TreeModel::iterator iter = m_model_available_parts->append();
    (*iter)[m_columns_available_parts.m_col_item] = new LayoutGroup();
    iter = m_model_available_parts->append();
    (*iter)[m_columns_available_parts.m_col_item] = new LayoutItem_Field();

    m_treeview_available_parts->set_model(m_model_available_parts);

    // Append the View columns:
    // Use set_cell_data_func() to give more control over the cell attributes depending on the row:

    //Name column:
    Gtk::TreeView::Column* column_part = Gtk::manage( new Gtk::TreeView::Column(_("Part")) );
    m_treeview_available_parts->append_column(*column_part);

    Gtk::CellRendererText* renderer_part = Gtk::manage(new Gtk::CellRendererText);
    column_part->pack_start(*renderer_part);
    column_part->set_cell_data_func(*renderer_part, sigc::mem_fun(*this, &Dialog_Layout_Report::on_cell_data_available_part));

    m_treeview_available_parts->set_headers_visible(false); //There's only one column, so this is not useful.
  }

  refGlade->get_widget("treeview_parts", m_treeview_parts);
  if(m_treeview_parts)
  {
    //Allow drag-and-drop:
    m_treeview_parts->enable_model_drag_source();
    m_treeview_parts->enable_model_drag_dest();

    m_model_parts = Gtk::TreeStore::create(m_columns_parts);
    m_treeview_parts->set_model(m_model_parts);

    // Append the View columns:
    // Use set_cell_data_func() to give more control over the cell attributes depending on the row:

    //Name column:
    Gtk::TreeView::Column* column_part = Gtk::manage( new Gtk::TreeView::Column(_("Part")) );
    m_treeview_parts->append_column(*column_part);

    Gtk::CellRendererText* renderer_part = Gtk::manage(new Gtk::CellRendererText);
    column_part->pack_start(*renderer_part);
    column_part->set_cell_data_func(*renderer_part, sigc::mem_fun(*this, &Dialog_Layout_Report::on_cell_data_part));

    //Connect to its signal:
    //renderer_name->property_editable() = true;
    //renderer_name->signal_edited().connect(
    //  sigc::bind( sigc::mem_fun(*this, &Dialog_Layout_Report::on_treeview_cell_edited_text), m_model_parts->m_columns.m_col_name) );


    //Connect to its signal:
    //renderer_count->signal_edited().connect(
    //  sigc::bind( sigc::mem_fun(*this, &Dialog_Layout_Report::on_treeview_cell_edited_numeric), m_model_parts->m_columns.m_col_columns_count) );

    //Respond to changes of selection:
    Glib::RefPtr<Gtk::TreeView::Selection> refSelection = m_treeview_parts->get_selection();
    if(refSelection)
    {
      refSelection->signal_changed().connect( sigc::mem_fun(*this, &Dialog_Layout_Report::on_treeview_fields_selection_changed) );
    }

    //m_model_parts->signal_row_changed().connect( sigc::mem_fun(*this, &Dialog_Layout_Report::on_treemodel_row_changed) );
  }

  refGlade->get_widget("button_up", m_button_up);
  m_button_up->signal_clicked().connect( sigc::mem_fun(*this, &Dialog_Layout_Report::on_button_up) );

  refGlade->get_widget("button_down", m_button_down);
  m_button_down->signal_clicked().connect( sigc::mem_fun(*this, &Dialog_Layout_Report::on_button_down) );

  refGlade->get_widget("button_delete", m_button_delete);
  m_button_delete->signal_clicked().connect( sigc::mem_fun(*this, &Dialog_Layout_Report::on_button_delete) );

  refGlade->get_widget("button_add", m_button_add);
  m_button_add->signal_clicked().connect( sigc::mem_fun(*this, &Dialog_Layout_Report::on_button_add) );

  refGlade->get_widget("button_edit", m_button_edit);
  m_button_edit->signal_clicked().connect( sigc::mem_fun(*this, &Dialog_Layout_Report::on_button_edit) );

  show_all_children();
}

Dialog_Layout_Report::~Dialog_Layout_Report()
{
}

void Dialog_Layout_Report::fill_group(const Gtk::TreeModel::iterator& iter, LayoutGroup& group)
{
  if(iter)
  {
    Gtk::TreeModel::Row row = *iter;

    LayoutItem* pItem = row[m_columns_parts.m_col_item];
    LayoutGroup* pGroup = dynamic_cast<LayoutGroup*>(pItem);
    if(pGroup)
      group = *pGroup;
/*
    //Get child layout items:
    for(Gtk::TreeModel::iterator iterChild = row.children().begin(); iterChild != row.children().end(); ++iterChild)
    {
      Gtk::TreeModel::Row rowChild = *iterChild;

      if(rowChild[m_columns_parts->m_columns.m_col_type] == TreeStore_Layout::TYPE_GROUP)
      {
        //Recurse:
        LayoutGroup group_child;
        fill_group(iterChild, group_child);
        group.add_item(group_child);
      }
      else if(rowChild[m_columns_parts->m_columns.m_col_type] == TreeStore_Layout::TYPE_PORTAL)
      {
        LayoutItem_Portal portal;
        portal.set_relationship(  rowChild[m_columns_parts->m_columns.m_col_relationship] );
        group.add_item(portal);
      }
      else if(rowChild[m_columns_parts->m_columns.m_col_type] == TreeStore_Layout::TYPE_FIELD)
      {
        //Add field:
        LayoutItem_Field field = rowChild[m_columns_parts->m_columns.m_col_field_formatting]; //TODO: Use _only_ this for fields in future?

        field.set_name( rowChild[m_columns_parts->m_columns.m_col_name] );

        field.m_relationship = rowChild[m_columns_parts->m_columns.m_col_relationship_name];

        //if(!relationship_name.empty())
        //{
        //  get_document()->get_table_relationship(m_table_name, field.m_relationship);
        //}

        field.set_editable( rowChild[m_columns_parts->m_columns.m_col_editable] );

        group.add_item(field);
      }
    }
*/
  }
}



void Dialog_Layout_Report::add_group(const Gtk::TreeModel::iterator& parent, const LayoutGroup& group)
{
  Gtk::TreeModel::iterator iterNewGroup;
  if(!parent)
  {
    //Add it at the top-level, because nothing was selected:
    iterNewGroup = m_model_parts->append();
  }
  else
  {
    iterNewGroup = m_model_parts->append(parent->children());
  }

  if(iterNewGroup)
  {
    Gtk::TreeModel::Row row = *iterNewGroup;
    row[m_columns_parts.m_col_item] = group.clone();

/*
    //Add the child items:
    LayoutGroup::type_map_const_items items = group.get_items();
    for(LayoutGroup::type_map_const_items::const_iterator iter = items.begin(); iter != items.end(); ++iter)
    {
      const LayoutItem* item = iter->second;
      const LayoutGroup* child_group = dynamic_cast<const LayoutGroup*>(item);
      if(child_group) //If it is a group:
        add_group(iterNewGroup, *child_group); //recursive
      else
      {
        const LayoutItem_Portal* portal = dynamic_cast<const LayoutItem_Portal*>(item);
        if(portal) //If it is a portal
        {
          Gtk::TreeModel::iterator iterField = m_model_parts->append(iterNewGroup->children());
          Gtk::TreeModel::Row row = *iterField;
          row[m_columns_parts->m_columns.m_col_type] = TreeStore_Layout::TYPE_PORTAL;
          row[m_columns_parts->m_columns.m_col_relationship] = portal->get_relationship();
        }
        else
        {
          const LayoutItem_Field* field = dynamic_cast<const LayoutItem_Field*>(item);
          if(field) //If it is a field
          {
            //Add the field to the treeview:
            Gtk::TreeModel::iterator iterField = m_model_parts->append(iterNewGroup->children());
            Gtk::TreeModel::Row row = *iterField;
            row[m_columns_parts->m_columns.m_col_type] = TreeStore_Layout::TYPE_FIELD;
            row[m_columns_parts->m_columns.m_col_field_formatting] = *field;
            row[m_columns_parts->m_columns.m_col_name] = field->get_name();
            row[m_columns_parts->m_columns.m_col_relationship_name] = field->m_relationship;

            row[m_columns_parts->m_columns.m_col_editable] = field->get_editable();
          }
        }
      }

    }
*/
    m_modified = true;
  }
}

void Dialog_Layout_Report::set_document(const Glib::ustring& layout, Document_Glom* document, const Glib::ustring& table_name, const type_vecLayoutFields& table_fields)
{
  m_modified = false;

  Dialog_Layout::set_document(layout, document, table_name, table_fields);


  //Update the tree models from the document
  if(document)
  {
    //Set the table name and title:
    m_label_table_name->set_text(table_name);
    //m_entry_table_title->set_text( document->get_table_title(table_name) );

    Document_Glom::type_mapLayoutGroupSequence mapGroups = document->get_data_layout_groups_plus_new_fields(layout, m_table_name);
    document->fill_layout_field_details(m_table_name, mapGroups); //Update with full field information.

    //If no information is stored in the document, then start with something:

    if(mapGroups.empty())
    {
      LayoutGroup group;
      group.set_name("main");
      group.m_columns_count = 2;

      guint field_sequence = 1; //0 means no sequence
      for(type_vecLayoutFields::const_iterator iter = table_fields.begin(); iter != table_fields.end(); ++iter)
      {
        LayoutItem_Field item = *iter;
        item.m_sequence = field_sequence;

        group.add_item(item, field_sequence);

        ++field_sequence;
      }

      mapGroups[1] = group;
    }

    //Show the field layout
    typedef std::list< Glib::ustring > type_listStrings;

    m_model_parts->clear();

    //guint field_sequence = 1; //0 means no sequence
    //guint group_sequence = 1; //0 means no sequence
    for(Document_Glom::type_mapLayoutGroupSequence::const_iterator iter = mapGroups.begin(); iter != mapGroups.end(); ++iter)
    {
      const LayoutGroup& group = iter->second;

      add_group(Gtk::TreeModel::iterator() /* null == top-level */, group);
    }

    //treeview_fill_sequences(m_model_parts, m_model_parts->m_columns.m_col_sequence); //The document should have checked this already, but it does not hurt to check again.
  }

  //Open all the groups:
  m_treeview_parts->expand_all();

  m_modified = false;
}

void Dialog_Layout_Report::enable_buttons()
{
  //Fields:
  Glib::RefPtr<Gtk::TreeView::Selection> refSelection = m_treeview_parts->get_selection();
  if(refSelection)
  {
    Gtk::TreeModel::iterator iter = refSelection->get_selected();
    if(iter)
    {
      //Disable Up if It can't go any higher.
      bool enable_up = true;
      if(iter == m_model_parts->children().begin())
        enable_up = false;  //It can't go any higher.

      m_button_up->set_sensitive(enable_up);


      //Disable Down if It can't go any lower.
      Gtk::TreeModel::iterator iterNext = iter;
      iterNext++;

      bool enable_down = true;
      if(iterNext == m_model_parts->children().end())
        enable_down = false;

      m_button_down->set_sensitive(enable_down);

      m_button_delete->set_sensitive(true);

      //m_button_formatting->set_sensitive( (*iter)[m_columns_parts->m_columns.m_col_type] == TreeStore_Layout::TYPE_FIELD);
    }
    else
    {
      //Disable all buttons that act on a selection:
      m_button_down->set_sensitive(false);
      m_button_up->set_sensitive(false);
      m_button_delete->set_sensitive(false);
    }
  }

}



void Dialog_Layout_Report::on_button_delete()
{
  Glib::RefPtr<Gtk::TreeSelection> refTreeSelection = m_treeview_parts->get_selection();
  if(refTreeSelection)
  {
    Gtk::TreeModel::iterator iter = refTreeSelection->get_selected();
    if(iter)
    {
      m_model_parts->erase(iter);

      m_modified = true;
    }
  }
}

void Dialog_Layout_Report::on_button_up()
{
  Glib::RefPtr<Gtk::TreeView::Selection> refSelection = m_treeview_parts->get_selection();
  if(refSelection)
  {
    Gtk::TreeModel::iterator iter = refSelection->get_selected();
    if(iter)
    {
      Gtk::TreeModel::iterator parent = iter->parent();
      bool is_first = false;
      if(parent)
        is_first = (iter == parent->children().begin());
      else
        is_first = (iter == m_model_parts->children().begin());

      if(!is_first)
      {
        Gtk::TreeModel::iterator iterBefore = iter;
        --iterBefore;

        m_model_parts->iter_swap(iter, iterBefore);

        m_modified = true;
      }
    }

  }

  enable_buttons();
}

void Dialog_Layout_Report::on_button_down()
{
  Glib::RefPtr<Gtk::TreeView::Selection> refSelection = m_treeview_parts->get_selection();
  if(refSelection)
  {
    Gtk::TreeModel::iterator iter = refSelection->get_selected();
    if(iter)
    {
      Gtk::TreeModel::iterator iterNext = iter;
      iterNext++;

      Gtk::TreeModel::iterator parent = iter->parent();
      bool is_last = false;
      if(parent)
        is_last = (iterNext == parent->children().end());
      else
        is_last = (iterNext == m_model_parts->children().end());

      if(!is_last)
      {
        //Swap the sequence values, so that the one before will be after:
        m_model_parts->iter_swap(iter, iterNext);

        m_modified = true;
      }
    }

  }

  enable_buttons();
}

void Dialog_Layout_Report::on_button_add()
{
  Gtk::TreeModel::iterator parent = get_selected_group_parent();
  Gtk::TreeModel::iterator available = get_selected_available();

g_warning("Dialog_Layout_Report::on_button_add");

  //Copy the available part to the list of parts:
  if(available)
  {
    const LayoutItem* pAvailable = (*available)[m_columns_available_parts.m_col_item];

    Gtk::TreeModel::iterator iter;
    if(parent)
      iter = m_model_parts->append(parent->children());
    else
      iter = m_model_parts->append();

    (*iter)[m_columns_parts.m_col_item] = pAvailable->clone();

g_warning("Dialog_Layout_Report::on_button_add: after add");
  }

  enable_buttons();
}


bool Dialog_Layout_Report::offer_relationship_list(Relationship& relationship)
{
  bool result = false;

  try
  {
    Glib::RefPtr<Gnome::Glade::Xml> refXml = Gnome::Glade::Xml::create(GLOM_GLADEDIR "glom.glade", "dialog_choose_relationship");

    Dialog_ChooseRelationship* dialog = 0;
    refXml->get_widget_derived("dialog_choose_relationship", dialog);

    if(dialog)
    {
      dialog->set_document(get_document(), m_table_name);
      dialog->set_transient_for(*this);
      int response = dialog->run();
      dialog->hide();
      if(response == Gtk::RESPONSE_OK)
      {
        //Get the chosen relationship:
        result = dialog->get_relationship_chosen(relationship);
      }

      delete dialog;
    }
  }
  catch(const Gnome::Glade::XmlError& ex)
  {
    std::cerr << ex.what() << std::endl;
  }

  return result;
}

bool Dialog_Layout_Report::offer_field_list(LayoutItem_Field& field)
{
  bool result = false;

  try
  {
    Glib::RefPtr<Gnome::Glade::Xml> refXml = Gnome::Glade::Xml::create(GLOM_GLADEDIR "glom.glade", "dialog_choose_field");

    Dialog_ChooseField* dialog = 0;
    refXml->get_widget_derived("dialog_choose_field", dialog);

    if(dialog)
    {
      dialog->set_document(get_document(), m_table_name, field);
      dialog->set_transient_for(*this);
      int response = dialog->run();
      if(response == Gtk::RESPONSE_OK)
      {
        //Get the chosen field:
        result = dialog->get_field_chosen(field);
      }

      delete dialog;
    }
  }
  catch(const Gnome::Glade::XmlError& ex)
  {
    std::cerr << ex.what() << std::endl;
  }

  return result;
}

bool Dialog_Layout_Report::offer_field_layout(LayoutItem_Field& field)
{
  bool result = false;

  try
  {
    Glib::RefPtr<Gnome::Glade::Xml> refXml = Gnome::Glade::Xml::create(GLOM_GLADEDIR "glom.glade", "dialog_layout_field_properties");

    Dialog_FieldLayout* dialog = 0;
    refXml->get_widget_derived("dialog_layout_field_properties", dialog);

    if(dialog)
    {
      add_view(dialog); //Give it access to the document.
      dialog->set_field(field, m_table_name);
      dialog->set_transient_for(*this);
      int response = dialog->run();
      if(response == Gtk::RESPONSE_OK)
      {
        //Get the chosen field:
        result = dialog->get_field_chosen(field);
      }

      remove_view(dialog);
      delete dialog;
    }
  }
  catch(const Gnome::Glade::XmlError& ex)
  {
    std::cerr << ex.what() << std::endl;
  }

  return result;
}

Gtk::TreeModel::iterator Dialog_Layout_Report::get_selected_group_parent() const
{
  //Get the selected group, or a suitable parent group, or the first group:

  Gtk::TreeModel::iterator parent;

  Glib::RefPtr<Gtk::TreeSelection> refTreeSelection = m_treeview_parts->get_selection();
  if(refTreeSelection)
  {
    Gtk::TreeModel::iterator iter = refTreeSelection->get_selected();
    if(iter)
    {
      Gtk::TreeModel::Row row = *iter;
      //if(row[m_columns_parts->m_columns.m_col_type] == TreeStore_Layout::TYPE_GROUP)
      //{
        //Add a group under this group:
      //  parent = iter;
      //}
      //else
      {
        //Add a group under this item's group:
        parent = iter->parent();
      }
    }
  }

  return parent;
}

Gtk::TreeModel::iterator Dialog_Layout_Report::get_selected_available() const
{
  //Get the selected group, or a suitable parent group, or the first group:

  Gtk::TreeModel::iterator iter;

  Glib::RefPtr<Gtk::TreeSelection> refTreeSelection = m_treeview_available_parts->get_selection();
  if(refTreeSelection)
  {
    iter = refTreeSelection->get_selected();
  }

  return iter;
}

void Dialog_Layout_Report::on_button_edit()
{
  //Get the selected item:
  Glib::RefPtr<Gtk::TreeSelection> refTreeSelection = m_treeview_parts->get_selection();
  if(refTreeSelection)
  {
    Gtk::TreeModel::iterator iter = refTreeSelection->get_selected();
    if(iter)
    {
      //Do something different for each type of item:
      //This is unpleasant, but so is this whole dialog.
      //This whole dialog is just a temporary way to edit the layout before we have a visual DnD way.
      Gtk::TreeModel::Row row = *iter;
/*
      switch( row[m_columns_parts->m_columns.m_col_type])
      {
        case TreeStore_Layout::TYPE_GROUP:
        {
          //TODO: Start editing the name.
          break;
        }
        case TreeStore_Layout::TYPE_FIELD:
        {
          LayoutItem_Field field;
          field.set_name( row[m_columns_parts->m_columns.m_col_name] ); //Start with this one selected.
          field.m_relationship = row[m_columns_parts->m_columns.m_col_relationship_name];

          //if(!relationship_name.empty())
          //{
          //  get_document()->get_table_relationship(m_table_name, field.m_relationship);
          //}

          field.set_editable( row[m_columns_parts->m_columns.m_col_editable] );
          bool test = offer_field_list(field);
          if(test)
          {
            row[m_columns_parts->m_columns.m_col_name] = field.get_name();
            row[m_columns_parts->m_columns.m_col_relationship_name] = field.m_relationship;

            row[m_columns_parts->m_columns.m_col_editable] = field.get_editable();
          }

          break;
        }
        case TreeStore_Layout::TYPE_PORTAL:
        {
          Relationship relationship;
          relationship.set_name( row[m_columns_parts->m_columns.m_col_relationship] ); //Start with this one selected.
          bool test = offer_relationship_list(relationship);
          if(test)
          {
            row[m_columns_parts->m_columns.m_col_relationship] = relationship.get_name();
          }

          break;
        }
      }
*/
    }
  }
}

void Dialog_Layout_Report::save_to_document()
{
  Dialog_Layout::save_to_document();

  if(m_modified)
  {
    //Set the table name and title:
    Document_Glom* document = get_document();
    //if(document)
    //  document->set_table_title( m_table_name, m_entry_table_title->get_text() );

    //Get the data from the TreeView and store it in the document:

    //Fill the sequences:
    //(This model is not sorted - we need to set the sequence numbers based on the order).
    //m_model_parts->fill_sequences();

    //Get the groups and their fields:
    Document_Glom::type_mapLayoutGroupSequence mapGroups;
    guint group_sequence = 1; //0 means no sequence

    //Add the layout items:
    //guint field_sequence = 1; //0 means no sequence
    for(Gtk::TreeModel::iterator iterFields = m_model_parts->children().begin(); iterFields != m_model_parts->children().end(); ++iterFields)
    {
      //Gtk::TreeModel::Row row = *iterFields;
      //if(row[m_columns_parts->m_columns.m_col_type] == TreeStore_Layout::TYPE_GROUP) //There may be top-level groups, but no top-level fields, because the fields must be in a group (so that they are in columns)
      {
        LayoutGroup group;
        group.m_sequence = group_sequence;
        fill_group(iterFields, group);

        mapGroups[group_sequence] = group;
        ++group_sequence;
      }
    }

    if(document)
    {
      document->set_data_layout_groups(m_layout_name, m_table_name, mapGroups);
      m_modified = false;
    }
  }
}

void Dialog_Layout_Report::on_treeview_fields_selection_changed()
{
  enable_buttons();
}

void Dialog_Layout_Report::on_cell_data_part(Gtk::CellRenderer* renderer, const Gtk::TreeModel::iterator& iter)
{
  //TODO: If we ever use this as a real layout tree, then let's add icons for each type.

  //Set the view's cell properties depending on the model's data:
  Gtk::CellRendererText* renderer_text = dynamic_cast<Gtk::CellRendererText*>(renderer);
  if(renderer_text)
  {
    if(iter)
    {
      Gtk::TreeModel::Row row = *iter;
      LayoutItem* pItem = row[m_columns_parts.m_col_item];
      Glib::ustring part = pItem->get_part_type_name();

      renderer_text->property_text() = part;
      renderer_text->property_editable() = false; //Part names can never be edited.
    }
  }
}

void Dialog_Layout_Report::on_cell_data_available_part(Gtk::CellRenderer* renderer, const Gtk::TreeModel::iterator& iter)
{
  //TODO: If we ever use this as a real layout tree, then let's add icons for each type.

  //Set the view's cell properties depending on the model's data:
  Gtk::CellRendererText* renderer_text = dynamic_cast<Gtk::CellRendererText*>(renderer);
  if(renderer_text)
  {
    if(iter)
    {
      Gtk::TreeModel::Row row = *iter;
      LayoutItem* pItem = row[m_columns_available_parts.m_col_item];
      Glib::ustring part = pItem->get_part_type_name();

      renderer_text->property_text() = part;
      renderer_text->property_editable() = false; //Part names can never be edited.
    }
  }
}

void Dialog_Layout_Report::on_treeview_cell_edited_text(const Glib::ustring& path_string, const Glib::ustring& new_text, const Gtk::TreeModelColumn<Glib::ustring>& model_column)
{
  if(!path_string.empty())
  {
    Gtk::TreePath path(path_string);

    //Get the row from the path:
    Gtk::TreeModel::iterator iter = m_model_parts->get_iter(path);
    if(iter)
    {
      //Store the user's new text in the model:
      Gtk::TreeRow row = *iter;
      row[model_column] = new_text;
    }
  }
}

void Dialog_Layout_Report::on_treeview_cell_edited_numeric(const Glib::ustring& path_string, const Glib::ustring& new_text, const Gtk::TreeModelColumn<guint>& model_column)
{
  //This is used on numerical model columns:
  if(path_string.empty())
    return;

  Gtk::TreePath path(path_string);

  //Get the row from the path:
  Gtk::TreeModel::iterator iter = m_model_parts->get_iter(path);
  if(iter)
  {
    //std::istringstream astream(new_text); //Put it in a stream.
    //ColumnType new_value = ColumnType();
    //new_value << astream; //Get it out of the stream as the numerical type.

    //Convert the text to a number, using the same logic used by GtkCellRendererText when it stores numbers.
    char* pchEnd = 0;
    guint new_value = static_cast<guint>( strtod(new_text.c_str(), &pchEnd) );

    //Don't allow a 0 columns_count:
    if(new_value == 0)
      new_value = 1;
      
    //Store the user's new text in the model:
    Gtk::TreeRow row = *iter;
    row[model_column] = (guint)new_value;
  }
}









