/* Glom
 *
 * Copyright (C) 2011 Openismus GmbH
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

// For instance:
// glom_test_connection --server-hostname=localhost --server-port=5433 --server-username=someuser

#include "config.h"

#include <libglom/connectionpool.h>
#include <libglom/connectionpool_backends/postgres_central.h>
#include <libglom/init.h>
#include <libglom/privs.h>
#include <libglom/utils.h>
#include <giomm/file.h>
#include <glibmm/optioncontext.h>
#include <glibmm/convert.h>
#include <iostream>

#include <glibmm/i18n.h>

class GlomCreateOptionGroup : public Glib::OptionGroup
{
public:
  GlomCreateOptionGroup();

  //These instances should live as long as the OptionGroup to which they are added,
  //and as long as the OptionContext to which those OptionGroups are added.
  bool m_arg_version;
  Glib::ustring m_arg_server_hostname;
  double m_arg_server_port;
  Glib::ustring m_arg_server_username;
  Glib::ustring m_arg_server_password;
  Glib::ustring m_arg_server_database;
};

GlomCreateOptionGroup::GlomCreateOptionGroup()
: Glib::OptionGroup("glom_create_from_example", _("Glom options"), _("Command-line options")),
  m_arg_version(false),
  m_arg_server_port(0)
{
  Glib::OptionEntry entry;

  entry.set_long_name("version");
  entry.set_short_name('V');
  entry.set_description(_("The version of this application."));
  add_entry(entry, m_arg_version);

  entry.set_long_name("server-hostname");
  entry.set_short_name('h');
  entry.set_description(_("The hostname of the PostgreSQL server, such as localhost."));
  add_entry(entry, m_arg_server_hostname);
  
  entry.set_long_name("server-port");
  entry.set_short_name('p');
  entry.set_description(_("The port of the PostgreSQL server, such as 5434."));
  add_entry(entry, m_arg_server_port);
  
  entry.set_long_name("server-username");
  entry.set_short_name('u');
  entry.set_description(_("The username for the PostgreSQL server."));
  add_entry(entry, m_arg_server_username);

  //Optional:
  entry.set_long_name("server-database");
  entry.set_short_name('d');
  entry.set_description(_("The specific database on the PostgreSQL server (Optional)."));
  add_entry(entry, m_arg_server_database);
}

static void print_options_hint()
{
  //TODO: How can we just print them out?
  std::cout << _("Use --help to see a list of available command-line options.") << std::endl;
}


int main(int argc, char* argv[])
{
  Glom::libglom_init();
  
  Glib::OptionContext context;
  GlomCreateOptionGroup group;
  context.set_main_group(group);
  
  try
  {
    context.parse(argc, argv);
  }
  catch(const Glib::OptionError& ex)
  {
    std::cout << _("Error while parsing command-line options: ") << std::endl << ex.what() << std::endl;
    print_options_hint();
    return EXIT_FAILURE;
  }
  catch(const Glib::Error& ex)
  {
    std::cerr << "Error: " << ex.what() << std::endl;
    return EXIT_FAILURE;
  }

  if(group.m_arg_version)
  {
    std::cout << PACKAGE_STRING << std::endl;
    return EXIT_SUCCESS;
  }

  if(group.m_arg_server_hostname.empty())
  {
    std::cerr << "Please provide a database hostname." << std::endl;
    print_options_hint();
    return EXIT_FAILURE;
  }

  if(group.m_arg_server_username.empty())
  {
    std::cerr << _("Please provide a database username.") << std::endl;
    print_options_hint();
    return EXIT_FAILURE;
  }

  //Get the password from stdin.
  //This is not a command-line option because then it would appear in logs.
  //Other command-line utilities such as psql don't do this either.
  //TODO: Support alternatives such as using a file.
  const Glib::ustring prompt = Glib::ustring::compose(
    _("Please enter the PostgreSQL server's password for the user %1: "), group.m_arg_server_username);
  const char* password = ::getpass(prompt.c_str());


  //Setup the connection, assuming that we are testing central hosting:
  Glom::ConnectionPool* connection_pool = Glom::ConnectionPool::get_instance();

  //Specify the backend and backend-specific details to be used by the connectionpool.
  //This is usually done by ConnectionPool::setup_from_document():
  Glom::ConnectionPoolBackends::PostgresCentralHosted* backend = 
    new Glom::ConnectionPoolBackends::PostgresCentralHosted;
  backend->set_host(group.m_arg_server_hostname);

  //Use a specified port, or try all suitable ports:
  if(group.m_arg_server_port)
  {
    backend->set_port(group.m_arg_server_port);
    backend->set_try_other_ports(false);
  }
  else
  {
    backend->set_try_other_ports(true);
  }

  connection_pool->set_user(group.m_arg_server_username);
  connection_pool->set_password(password);
  connection_pool->set_backend(std::auto_ptr<Glom::ConnectionPool::Backend>(backend));

  if(group.m_arg_server_database.empty())
  {
    //Prevent it from trying to connect to a database with the same name as the user,
    //which is more likely to exist by chance than this silly name:
    connection_pool->set_database("somenonexistantdatbasename");
  }
  else
  {
    connection_pool->set_database(group.m_arg_server_database);
  }

  connection_pool->set_ready_to_connect();

  try
  {
    connection_pool->connect();
  }
  catch(const Glom::ExceptionConnection& ex)
  {
    if(ex.get_failure_type() == Glom::ExceptionConnection::FAILURE_NO_SERVER)
    {
      std::cerr << _("Error: Could not connect to the server even without specifying a database.") << std::endl;
      return EXIT_FAILURE;
    }
    else if(ex.get_failure_type() == Glom::ExceptionConnection::FAILURE_NO_DATABASE)
    {
      //We expect this exception if we did not specify a database:
      if(!(group.m_arg_server_database.empty()))
      {  
        std::cerr << _("Error: Could not connect to the specified database.") << std::endl;
        return EXIT_FAILURE;
      }
    }
  }

  std::cout << _("Successful connection.") << std::endl;
        
  Glom::libglom_deinit();

  return EXIT_SUCCESS;
}