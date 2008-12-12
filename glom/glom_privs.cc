/* Glom
 *
 * Copyright (C) 2001-2006 Murray Cumming
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

#include "glom_privs.h"
#include <glom/libglom/standard_table_prefs_fields.h>
#include <glom/application.h>

namespace Glom
{

Privs::type_vecStrings Privs::get_database_groups()
{
  type_vecStrings result;

  Glib::ustring strQuery = "SELECT \"pg_group\".\"groname\" FROM \"pg_group\"";
  Glib::RefPtr<Gnome::Gda::DataModel> data_model = query_execute(strQuery);
  if(data_model)
  {
    const int rows_count = data_model->get_n_rows();
    for(int row = 0; row < rows_count; ++row)
    {
      const Gnome::Gda::Value value = data_model->get_value_at(0, row);
      const Glib::ustring name = value.get_string();
      result.push_back(name);
    }
  }

  return result;
}

Privs::type_vecStrings Privs::get_database_users(const Glib::ustring& group_name)
{
  Bakery::BusyCursor cursor(App_Glom::get_application());

  type_vecStrings result;

  if(group_name.empty())
  {
    //pg_shadow contains the users. pg_users is a view of pg_shadow without the password.
    Glib::ustring strQuery = "SELECT \"pg_shadow\".\"usename\" FROM \"pg_shadow\"";
    Glib::RefPtr<Gnome::Gda::DataModel> data_model = query_execute(strQuery);
    if(data_model)
    {
      const int rows_count = data_model->get_n_rows();
      for(int row = 0; row < rows_count; ++row)
      {
        const Gnome::Gda::Value value = data_model->get_value_at(0, row);
        const Glib::ustring name = value.get_string();
        result.push_back(name);
      }
    }
  }
  else
  {
    Glib::ustring strQuery = "SELECT \"pg_group\".\"groname\", \"pg_group\".\"grolist\" FROM \"pg_group\" WHERE \"pg_group\".\"groname\" = '" + group_name + "'";
    Glib::RefPtr<Gnome::Gda::DataModel> data_model = query_execute(strQuery);
    if(data_model && data_model->get_n_rows())
    {
      const int rows_count = data_model->get_n_rows();
      for(int row = 0; row < rows_count; ++row)
      {
        const Gnome::Gda::Value value = data_model->get_value_at(1, row); //Column 1 is the /* the user list.
        //pg_group is a string, formatted, bizarrely, like so: "{100, 101}".

        Glib::ustring group_list;
        if(!value.is_null())
          group_list = value.get_string();

        type_vecStrings vecUserIds = pg_list_separate(group_list);
        for(type_vecStrings::const_iterator iter = vecUserIds.begin(); iter != vecUserIds.end(); ++iter)
        {
          //TODO_Performance: Can we do this in one SQL SELECT?
          Glib::ustring strQuery = "SELECT \"pg_user\".\"usename\" FROM \"pg_user\" WHERE \"pg_user\".\"usesysid\" = '" + *iter + "'";
          Glib::RefPtr<Gnome::Gda::DataModel> data_model = query_execute(strQuery);
          if(data_model)
          {
            const Gnome::Gda::Value value = data_model->get_value_at(0, 0); 
            result.push_back(value.get_string());
          }
        }

      }
    }
  }

  return result;
}

void Privs::set_table_privileges(const Glib::ustring& group_name, const Glib::ustring& table_name, const Privileges& privs, bool developer_privs)
{
  if(group_name.empty() || table_name.empty())
    return;

  //Change the permission in the database:

  //Build the SQL statement:

  //Grant or revoke:
  Glib::ustring strQuery = "GRANT";
  //TODO: Revoke the ones that are not specified.

  //What to grant or revoke:
  Glib::ustring strPrivilege;

  if(developer_privs)
    strPrivilege = "ALL PRIVILEGES";
  else
  {
    if(privs.m_view)
      strPrivilege += "SELECT";

    if(privs.m_edit)
    {
      if(!strPrivilege.empty())
        strPrivilege += ", ";

      strPrivilege += "UPDATE";
    }

    if(privs.m_create)
    {
      if(!strPrivilege.empty())
        strPrivilege += ", ";

      strPrivilege += "INSERT";
    }

    if(privs.m_delete)
    {
      if(!strPrivilege.empty())
        strPrivilege += ", ";

      strPrivilege += "DELETE";
    }
  }

  strQuery += " " + strPrivilege + " ON \"" + table_name + "\" ";

  //This must match the Grant or Revoke:
  strQuery += "TO";

  strQuery += " GROUP \"" + group_name + "\"";

  const bool test = query_execute(strQuery);

  if(test)
  {
    if( (table_name != GLOM_STANDARD_TABLE_AUTOINCREMENTS_TABLE_NAME) && privs.m_create )
    {
      //To create a record, you will usually need write access to the autoincrements table,
      //so grant this too:
      Privileges priv_autoincrements;
      priv_autoincrements.m_view = true;
      priv_autoincrements.m_edit = true;
      set_table_privileges(group_name, GLOM_STANDARD_TABLE_AUTOINCREMENTS_TABLE_NAME, priv_autoincrements);
    }
  }
}

Privileges Privs::get_table_privileges(const Glib::ustring& group_name, const Glib::ustring& table_name)
{
  Privileges result;

  if(group_name == GLOM_STANDARD_GROUP_NAME_DEVELOPER)
  {
    //Always give developers full access:
    result.m_view = true;
    result.m_edit = true;
    result.m_create = true;
    result.m_delete = true;
    return result;
  }

  //Get the permissions:
  Glib::ustring strQuery = "SELECT \"pg_class\".\"relacl\" FROM \"pg_class\" WHERE \"pg_class\".\"relname\" = '" + table_name + "'";
  Glib::RefPtr<Gnome::Gda::DataModel> data_model = query_execute(strQuery);
  if(data_model && data_model->get_n_rows())
  {
    const Gnome::Gda::Value value = data_model->get_value_at(0, 0);
    Glib::ustring access_details;
    if(!value.is_null())
      access_details = value.get_string();

    //std::cout << "DEBUG: access_details:" << access_details << std::endl;

    //Parse the strange postgres permissions format:
    //For instance, "{murrayc=arwdxt/murrayc,operators=r/murrayc}" (Postgres 8.x)
    //For instance, "{murrayc=arwdxt/murrayc,group operators=r/murrayc}" (Postgres <8.x)
    const type_vecStrings vecItems = pg_list_separate(access_details);
    for(type_vecStrings::const_iterator iterItems = vecItems.begin(); iterItems != vecItems.end(); ++iterItems)
    {
      Glib::ustring item = *iterItems;
      //std::cout << "DEBUG: item:" << item << std::endl;

      item = Utils::string_trim(item, "\""); //Remove quotes from front and back.

      //std::cout << "DEBUG: item without quotes:" << item << std::endl;

      //Find group permissions, ignoring user permissions:
      //We need to find the role by name.
      // Previous versions of Postgres (8.1, or maybe 7.4) prefixed group names by "group ", 
      // but that doesn't work for recent versions of Postgres,
      // probably because the user and group concepts have been combined into "roles".
      //
      //const Glib::ustring strgroup = "group ";
      const Glib::ustring strgroup = group_name + "=";
      Glib::ustring::size_type posFind = item.find(strgroup);
      if(posFind != Glib::ustring::npos)
      {
        //It is the needed group permision:

        //Remove the "group " prefix (not needed for Postgres 8.x):
        //item = item.substr(strgroup.size());
        item = item.substr(posFind);
        //std::cout << "DEBUG: user permissions:" << item << std::endl;

        //Get the parts before and after the =:
        const type_vecStrings vecParts = Utils::string_separate(item, "=");
        if(vecParts.size() == 2)
        {
          const Glib::ustring this_group_name = vecParts[0];
          if(this_group_name == group_name) //Only look at permissions for the requested group->
          {
            Glib::ustring group_permissions = vecParts[1];

            //Get the part before the /user_who_granted_the_privileges:
            const type_vecStrings vecParts = Utils::string_separate(group_permissions, "/");
            if(!vecParts.empty())
              group_permissions = vecParts[0];

            //g_warning("  group=%s", group_name.c_str());
            //g_warning("  permisisons=%s", group_permissions.c_str());

            //Iterate through the characters:
            for(Glib::ustring::iterator iter = group_permissions.begin(); iter != group_permissions.end(); ++iter)
            {
              gunichar chperm = *iter;
              Glib::ustring perm(1, chperm);

              //See http://www.postgresql.org/docs/8.0/interactive/sql-grant.html
              if(perm == "r")
                result.m_view = true;
              else if(perm == "w")
                result.m_edit = true;
              else if(perm == "a")
                result.m_create = true;
              else if(perm == "d")
                result.m_delete = true;
            }
          }
        }
      }

    }
  }

  //g_warning("get_table_privileges(group_name=%s, table_name=%s) returning: %d", group_name.c_str(), table_name.c_str(), result.m_create);
  return result;
}


Glib::ustring Privs::get_user_visible_group_name(const Glib::ustring& group_name)
{
  Glib::ustring result = group_name;

  //Remove the special prefix:
  const Glib::ustring prefix = "glom_";
  if(result.substr(0, prefix.size()) == prefix)
    result = result.substr(prefix.size());

  return result;
}

Base_DB::type_vecStrings Privs::get_groups_of_user(const Glib::ustring& user)
{
  //TODO_Performance

  type_vecStrings result;

  //Look at each group:
  type_vecStrings groups = get_database_groups();
  for(type_vecStrings::const_iterator iter = groups.begin(); iter != groups.end(); ++iter)
  {
    //See whether the user is in this group:
    if(get_user_is_in_group(user, *iter))
    {
      //Add the group to the result:
      result.push_back(*iter);
    }
  }

  return result;
}

bool Privs::get_user_is_in_group(const Glib::ustring& user, const Glib::ustring& group)
{
  const type_vecStrings users = get_database_users(group);
  type_vecStrings::const_iterator iterFind = std::find(users.begin(), users.end(), user);
  return (iterFind != users.end());
}

Privileges Privs::get_current_privs(const Glib::ustring& table_name)
{
  //TODO_Performance: There's lots of database access here.
  //We could maybe replace some with the postgres has_table_* function().

  Bakery::BusyCursor cursor(App_Glom::get_application());

  Privileges result;

  ConnectionPool* connection_pool = ConnectionPool::get_instance();
  const Glib::ustring current_user = connection_pool->get_user();

  //Is the user in the special developers group?
  /*
  type_vecStrings developers = get_database_users(GLOM_STANDARD_GROUP_NAME_DEVELOPER);
  type_vecStrings::const_iterator iterFind = std::find(developers.begin(), developers.end(), current_user);
  if(iterFind != developers.end())
  {
    result.m_developer = true;
  }
  */

  sharedptr<SharedConnection> sharedconnection = connection_pool->connect();
  if(sharedconnection && sharedconnection->get_gda_connection()->supports_feature(Gnome::Gda::CONNECTION_FEATURE_USERS))
  {
    //Get the "true" rights for any groups that the user is in:
    type_vecStrings groups = get_groups_of_user(current_user);
    for(type_vecStrings::const_iterator iter = groups.begin(); iter != groups.end(); ++iter)
    {
      Privileges privs = get_table_privileges(*iter, table_name);

      if(privs.m_view)
        result.m_view = true;

      if(privs.m_edit)
        result.m_edit = true;

      if(privs.m_create)
        result.m_create = true;

      if(privs.m_delete)
        result.m_delete = true;
    }
  }
  else
  {
    // If the database doesn't support users we have privileges to do everything
    result.m_view = true;
    result.m_edit = true;
    result.m_create = true;
    result.m_delete = true;
  }

  return result;
}

} //namespace Glom
