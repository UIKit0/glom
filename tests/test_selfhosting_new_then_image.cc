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
#include "tests/test_utils.h"
#include "tests/test_utils_images.h"
#include <libglom/init.h>
#include <libglom/utils.h>
#include <libglom/db_utils.h>
#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>
#include <libgda/gda-blob-op.h>
#include <glib.h> //For g_assert()
#include <iostream>
#include <cstdlib> //For EXIT_SUCCESS and EXIT_FAILURE
#include <cstring> //For memcmp().

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
  

  //Where clause:

  const Glib::ustring table_name = "contacts";
  const Glom::sharedptr<const Glom::Field> field = document.get_field(table_name, "picture");

  //Where clause:
  const Glom::sharedptr<const Glom::Field> key_field = document.get_field(table_name, "contact_id");
  if(!key_field)
  {
    std::cerr << G_STRFUNC << ": Failure: Could not get key field." << std::endl;
    return false;
  }

  const Gnome::Gda::SqlExpr where_clause = 
    Glom::Utils::build_simple_where_expression(table_name, key_field, Gnome::Gda::Value(1));

  //Set the value, from an image file:
  const Gnome::Gda::Value value_set = get_value_for_image();
  g_assert(check_value_is_an_image(value_set));
  const Glib::RefPtr<const Gnome::Gda::SqlBuilder> builder_set = 
    Glom::Utils::build_sql_update_with_where_clause(table_name,
      field, value_set, where_clause);
  const int rows_affected = Glom::DbUtils::query_execute(builder_set);
  if(rows_affected == -1)
  {
    std::cerr << G_STRFUNC << ": Failure: UPDATE failed." << std::endl;
    return false;
  }


  //Get the value:
  Glom::Utils::type_vecLayoutFields fieldsToGet;
  Glom::sharedptr<Glom::LayoutItem_Field> layoutitem = Glom::sharedptr<Glom::LayoutItem_Field>::create();
  layoutitem->set_full_field_details(field);
  fieldsToGet.push_back(layoutitem);

  const Glib::RefPtr<const Gnome::Gda::SqlBuilder> builder_get = 
    Glom::Utils::build_sql_select_with_where_clause(table_name,
      fieldsToGet, where_clause);
  Glib::RefPtr<Gnome::Gda::DataModel> data_model = 
    Glom::DbUtils::query_execute_select(builder_get);
  if(!test_model_expected_size(data_model, 1, 1))
  {
    std::cerr << G_STRFUNC << ": Failure: Unexpected data model size for main query." << std::endl;
    return false;
  }

  const int count = Glom::DbUtils::count_rows_returned_by(builder_get);
  if(count != 1 )
  {
    std::cerr << G_STRFUNC << ": Failure: The COUNT query returned an unexpected value: " << count << std::endl;
    return false;
  }
  
  const Gnome::Gda::Value value_read = data_model->get_value_at(0, 0);
  const GType value_read_type = value_read.get_value_type();
  if( (value_read_type != GDA_TYPE_BINARY) &&
    (value_read_type != GDA_TYPE_BLOB))
  {
    std::cerr << G_STRFUNC << ": Failure: The value read was not of the expected type: " << g_type_name( value_read.get_value_type() ) << std::endl;
    return false;
  }

  //Make sure that we have a GdaBinary,
  //even if (as with SQLite) it's actually a GdaBlob that we get back:
  const GdaBinary* binary_read = 0;
  if(value_read_type == GDA_TYPE_BINARY)
    binary_read = gda_value_get_binary(value_read.gobj());
  else if(value_read_type == GDA_TYPE_BLOB)
  {
    const GdaBlob* blob = gda_value_get_blob(value_read.gobj());
    const bool read_all = gda_blob_op_read_all(const_cast<GdaBlobOp*>(blob->op), const_cast<GdaBlob*>(blob));
    if(!read_all)
    {
      std::cerr << G_STRFUNC << ": Failure: gda_blob_op_read_all() failed." << std::endl;
      return false;
    }

    binary_read = &(blob->data);
  }

  const GdaBinary* binary_set = gda_value_get_binary(value_set.gobj());
  if(!binary_set)
  {
    std::cerr << G_STRFUNC << ": Failure: The value read's data was null." << std::endl;
    return false;
  }

  if(binary_set->binary_length != binary_read->binary_length)
  {
    std::cerr << G_STRFUNC << ": Failure: The value read's data length was not equal to that of the value set." << std::endl;
    return false;
  }

  if(memcmp(binary_set->data, binary_read->data, binary_set->binary_length) != 0)
  {
    std::cerr << G_STRFUNC << ": Failure: The value read was not equal to the value set." << std::endl;
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
