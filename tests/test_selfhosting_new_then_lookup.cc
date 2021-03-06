/* Glom
 *
 * Copyright (C) 2010 Openismus GmbH
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

#include "tests/test_selfhosting_utils.h"
#include <libglom/init.h>
#include <libglom/utils.h>
#include <libglom/db_utils.h>
#include <libglom/connectionpool.h>
#include <libglom/data_structure/glomconversions.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>
#include <glib.h> //For g_assert()
#include <iostream>
#include <cstdlib> //For EXIT_SUCCESS and EXIT_FAILURE

static Glom::sharedptr<const Glom::LayoutItem_Field> get_lookup_field(const Glom::Document::type_list_lookups& container, const Glib::ustring& table_name, const Glib::ustring& field_name, Glom::sharedptr<const Glom::Relationship>& relationship)
{
  relationship.clear();
  Glom::sharedptr<const Glom::LayoutItem_Field> result;

  for(Glom::Document::type_list_lookups::const_iterator iter = container.begin(); iter != container.end(); ++iter)
  {
    const Glom::sharedptr<const Glom::LayoutItem_Field> layout_item = iter->first;
    if(!layout_item)
      return result;

    const Glom::sharedptr<const Glom::Relationship> this_relationship = iter->second;
    if(!this_relationship)
      return result;

    if(layout_item->get_table_used(table_name) != table_name)
      return result;

    if(layout_item->get_name() == field_name)
    {
      relationship = this_relationship;
      return layout_item;
    }
  }

  return result;
}

static bool contains_field(const Glom::Document::type_list_lookups& container, const Glib::ustring& table_name, const Glib::ustring& field_name)
{
  Glom::sharedptr<const Glom::Relationship> relationship;
  return get_lookup_field(container, table_name, field_name, relationship);
}

static bool test(Glom::Document::HostingMode hosting_mode)
{
  Glom::Document document;
  const bool recreated = 
    test_create_and_selfhost_from_example("example_smallbusiness.glom", document, hosting_mode);
  if(!recreated)
  {
    std::cerr << G_STRFUNC << ": Recreation failed." << std::endl;
    return false;
  }
  
  const Glib::ustring table_name = "invoice_lines";
  Glom::sharedptr<const Glom::Field> primary_key_field = document.get_field_primary_key(table_name);
  if(!primary_key_field)
  {
    std::cerr << G_STRFUNC << ": Failure: primary_key_field is empty." << std::endl;
    return false;
  }


  // Get the fields whose values should be looked up when a field changes:
  const Glom::Document::type_list_lookups lookups = document.get_lookup_fields(table_name, "product_id");
  if(lookups.size() != 3)
  {
    std::cerr << G_STRFUNC << ": Failure: Unexpected number of lookups: " << lookups.size() << std::endl;
    return false;
  }

  if(!contains_field(lookups, table_name, "product_price"))
  {
    std::cerr << G_STRFUNC << ": Failure: Expected lookup field not found." << std::endl;
    return false;
  }

  if(!contains_field(lookups, table_name, "product_name"))
  {
    std::cerr << G_STRFUNC << ": Failure: Expected lookup field not found." << std::endl;
    return false;
  }

  if(!contains_field(lookups, table_name, "vat_percentage"))
  {
    std::cerr << G_STRFUNC << ": Failure: Expected lookup field not found." << std::endl;
    return false;
  }

  const Glib::ustring field_name = "product_price";
  Glom::sharedptr<const Glom::Relationship> relationship;
  const Glom::sharedptr<const Glom::LayoutItem_Field> layout_field = 
    get_lookup_field(lookups, table_name, field_name, relationship);
  if(!layout_field)
  {
    std::cerr << G_STRFUNC << ": Failure: The lookup field is empty." << std::endl;
    return false;
  }
 
  if(!relationship)
  {
    std::cerr << G_STRFUNC << ": Failure: The lookup relationship is empty." << std::endl;
    return false;
  }

  if(relationship->get_to_table() != "products")
  {
    std::cerr << G_STRFUNC << ": Failure: The relationship's to table is unexpected." << std::endl;
    return false;
  }

  if(layout_field->get_table_used(table_name) != table_name)
  {
    std::cerr << G_STRFUNC << ": Failure: The lookup field's table is unexpected" << std::endl;
    return false;
  }

  if(layout_field->get_name() != field_name)
  {
    std::cerr << G_STRFUNC << ": Failure: The lookup field's name is unexpected." << std::endl;
    return false;
  }

  const Glom::sharedptr<const Glom::Field> field = layout_field->get_full_field_details();
  if(!field)
  {
    std::cerr << G_STRFUNC << ": Failure: The lookup item's field is empty." << std::endl;
    return false;
  }

  if(field->get_name() != field_name)
  {
    std::cerr << G_STRFUNC << ": Failure: The lookup item's field name is unexpected." << std::endl;
    return false;
  }

  if(!field->get_is_lookup())
  {
    std::cerr << G_STRFUNC << ": Failure: The lookup item's field is not a lookup." << std::endl;
    return false;
  }

  if(field->get_lookup_field() != "price")
  {
    std::cerr << G_STRFUNC << ": Failure: The lookup item's field's name is unexpected." << std::endl;
    return false;
  }

  if(relationship != field->get_lookup_relationship())
  {
    std::cerr << G_STRFUNC << ": Failure: The lookup item's field's relationship is not expected." << std::endl;
    return false;
  }

  //Lookup the value from the related record.
  const Glom::sharedptr<Glom::Field> field_source = 
    document.get_field(relationship->get_to_table(), field->get_lookup_field());
  const Gnome::Gda::Value value = Glom::DbUtils::get_lookup_value(&document, 
    table_name, relationship, field_source, Gnome::Gda::Value(2));

  if(!test_check_numeric_value_type(hosting_mode, value))
  {
    std::cerr << G_STRFUNC << ": Failure: The value has an unexpected type: " << 
      g_type_name(value.get_value_type()) << std::endl;
    return false;
  }

  if(Glom::Conversions::get_double_for_gda_value_numeric(value) != 3.5f)
  {
    std::cerr << G_STRFUNC << ": Failure: The value has an unexpected value: " << value.to_string() << " instead of 3.5" << std::endl;
    std::cerr << G_STRFUNC << ":     value as string: " << value.to_string() << std::endl;
    std::cerr << G_STRFUNC << ":     value GType: " << g_type_name(value.get_value_type()) << std::endl;
    return false;
  }

  test_selfhosting_cleanup();
 
  return true; 
}

int main()
{
  Glom::libglom_init();
  
  const int result = test_all_hosting_modes(sigc::ptr_fun(&test));

  Glom::libglom_deinit();

  return result;
}
