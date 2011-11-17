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
71 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "tests/test_selfhosting_utils.h"
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
    std::cerr << "Recreation failed." << std::endl;
    return false;
  }
  

  //Where clause:

  const Glib::ustring table_name = "contacts";
  const Glom::sharedptr<const Glom::Field> field = document.get_field(table_name, "picture");

  //Where clause:
  const Glom::sharedptr<const Glom::Field> key_field = document.get_field(table_name, "contact_id");
  if(!key_field)
  {
    std::cerr << "Failure: Could not get key field." << std::endl;
    return false;
  }

  const Gnome::Gda::SqlExpr where_clause = 
    Glom::Utils::build_simple_where_expression(table_name, key_field, Gnome::Gda::Value(1));

  //Fill a value from a file:
  const std::string filename =
    Glib::build_filename(GLOM_TESTS_IMAGE_DATA_NOTINSTALLED, "test_image.jpg");
  std::string data;
  try
  {
    data = Glib::file_get_contents(filename);
  }
  catch(const Glib::Error& ex)
  {
    std::cerr << "Failed: file_get_contents() failed: " << ex.what() << std::endl;
    return false; //Something went wrong. It does not exist.
  }

  if(data.empty())
  {
    std::cerr << "Failed: The data read from the file was empty. filepath=" << filename << std::endl;
    return false;
  }

  //Set the value:
  Gnome::Gda::Value value_set((const guchar*)data.c_str(), data.size());
  const Glib::RefPtr<const Gnome::Gda::SqlBuilder> builder_set = 
    Glom::Utils::build_sql_update_with_where_clause(table_name,
      field, value_set, where_clause);
  const int rows_affected = Glom::DbUtils::query_execute(builder_set);
  if(rows_affected == -1)
  {
    std::cerr << "Failure: UPDATE failed." << std::endl;
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
    std::cerr << "Failure: Unexpected data model size for main query." << std::endl;
    return false;
  }

  const int count = Glom::DbUtils::count_rows_returned_by(builder_get);
  if(count != 1 )
  {
    std::cerr << "Failure: The COUNT query returned an unexpected value: " << count << std::endl;
    return false;
  }
  
  const Gnome::Gda::Value value_read = data_model->get_value_at(0, 0);
  const GType value_read_type = value_read.get_value_type();
  if( (value_read_type != GDA_TYPE_BINARY) &&
    (value_read_type != GDA_TYPE_BLOB))
  {
    std::cerr << "Failure: The value read was not of the expected type: " << g_type_name( value_read.get_value_type() ) << std::endl;
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
      std::cerr << "Failure: gda_blob_op_read_all() failed." << std::endl;
      return false;
    }

    binary_read = &(blob->data);
  }

  const GdaBinary* binary_set = gda_value_get_binary(value_set.gobj());
  if(binary_set->binary_length != binary_read->binary_length)
  {
    std::cerr << "Failure: The value read's data length was not equal to that of the value set." << std::endl;
    return false;
  }

  if(memcmp(binary_set->data, binary_read->data, binary_set->binary_length) != 0)
  {
    std::cerr << "Failure: The value read was not equal to the value set." << std::endl;
    return false;
  }

  test_selfhosting_cleanup();
 
  return true; 
}

int main()
{
  Glom::libglom_init();
  
  if(!test(Glom::Document::HOSTING_MODE_POSTGRES_SELF))
  {
    std::cerr << "Failed with PostgreSQL" << std::endl;
    test_selfhosting_cleanup();
    return EXIT_FAILURE;
  }
  
  if(!test(Glom::Document::HOSTING_MODE_SQLITE))
  {
    std::cerr << "Failed with SQLite" << std::endl;
    test_selfhosting_cleanup();
    return EXIT_FAILURE;
  }

  Glom::libglom_deinit();

  return EXIT_SUCCESS;
}