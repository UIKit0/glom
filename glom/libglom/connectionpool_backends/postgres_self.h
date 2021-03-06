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

#ifndef GLOM_BACKEND_POSTGRES_SELF_H
#define GLOM_BACKEND_POSTGRES_SELF_H

#include <libglom/libglom_config.h>

#include <libglom/connectionpool_backends/postgres.h>

namespace Glom
{

namespace ConnectionPoolBackends
{

class PostgresSelfHosted : public Postgres
{
public:
  PostgresSelfHosted();

  /** Return whether the self-hosted server is currently running.
   *
   * @result True if it is running, and false otherwise.
   */
  bool get_self_hosting_active() const;

  /** Returns the port number the local postgres server is running on.
   *
   * @result The port number of the self-hosted server, or 0 if it is not
   * running.
   */
  unsigned int get_port() const;

  /** Try to install postgres on the distro, though this will require a
   * distro-specific patch to the implementation.
   */
  static bool install_postgres(const SlotProgress& slot_progress);

private:
  virtual InitErrors initialize(const SlotProgress& slot_progress, const Glib::ustring& initial_username, const Glib::ustring& password, bool network_shared = false);

  virtual StartupErrors startup(const SlotProgress& slot_progress, bool network_shared = false);
  virtual bool cleanup(const SlotProgress& slot_progress);
  virtual bool set_network_shared(const SlotProgress& slot_progress, bool network_shared = true);

  virtual Glib::RefPtr<Gnome::Gda::Connection> connect(const Glib::ustring& database, const Glib::ustring& username, const Glib::ustring& password, bool fake_connection = false);

  virtual bool create_database(const SlotProgress& slot_progress, const Glib::ustring& database_name, const Glib::ustring& username, const Glib::ustring& password);

private:
  /** Examine ports one by one, starting at @a starting_port, in increasing
   * order, and return the first one that is available.
   */
  static unsigned int discover_first_free_port(unsigned int start_port, unsigned int end_port);

  /** Run the command-line with the --version option to discover what version
   * of PostgreSQL is installed, so we can use the appropriate configuration
   * options when self-hosting.
   */
  Glib::ustring get_postgresql_utils_version(const SlotProgress& slot_progress);

  float get_postgresql_utils_version_as_number(const SlotProgress& slot_progress);
  
  void show_active_connections();

  bool m_network_shared;
  
  //These are only remembered in order to use them to provide debug
  //information when the PostgreSQL shutdown fails:
  Glib::ustring m_saved_database_name, m_saved_username, m_saved_password;
};

} // namespace ConnectionPoolBackends

} //namespace Glom

#endif //GLOM_BACKEND_POSTGRES_SELF_H
