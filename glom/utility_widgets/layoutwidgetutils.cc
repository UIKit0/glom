/*
 * glom
 * 
 * glom is free software.
 * 
 * You may redistribute it and/or modify it under the terms of the
 * GNU General Public License, as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option)
 * any later version.
 * 
 * glom is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * See the GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with glom.  If not, write to:
 * 	The Free Software Foundation, Inc.,
 * 	51 Franklin Street, Fifth Floor
 * 	Boston, MA  02110-1301, USA.
 */

#include "layoutwidgetutils.h"
#include <gtkmm/builder.h>
#include <giomm/menu.h>
#include <glibmm/i18n.h>
#include <iostream>

namespace Glom
{
  
LayoutWidgetUtils::LayoutWidgetUtils() :
  m_pPopupMenuUtils(0)
{
  //Derived class's constructors must call this:
  //setup_util_menu(this);
}

LayoutWidgetUtils::~LayoutWidgetUtils()
{
	
}

#ifndef GLOM_ENABLE_CLIENT_ONLY
void LayoutWidgetUtils::setup_util_menu(Gtk::Widget* widget)
{
#ifndef GLOM_ENABLE_CLIENT_ONLY
  m_refActionGroup = Gio::SimpleActionGroup::create();

  m_refUtilProperties = m_refActionGroup->add_action("properties",
    sigc::mem_fun(*this, &LayoutWidgetUtils::on_menu_properties_activate) );
  m_refUtilDelete = m_refActionGroup->add_action("delete",
    sigc::mem_fun(*this, &LayoutWidgetUtils::on_menu_delete_activate) );
  
  widget->insert_action_group("utility", m_refActionGroup);

  Glib::RefPtr<Gtk::Builder> builder = Gtk::Builder::create();

  Glib::ustring ui_info =
    "<interface>"
    "  <menu id='UtilMenu'>"
    "    <section>"
    "      <item>"
    "        <attribute name='label' translatable='yes'>Properties</attribute>"
    "        <attribute name='action'>utility.properties</attribute>"
    "      </item>"
    "      <item>"
    "        <attribute name='label' translatable='yes'>_Delete</attribute>"
    "        <attribute name='action'>utility.delete</attribute>"
    "      </item>"
    "    </section>"
    "  </menu>"
    "</interface";

  try
  {
    builder->add_from_string(ui_info);
  }
  catch(const Glib::Error& ex)
  {
    std::cerr << G_STRFUNC << ": building menus failed: " <<  ex.what();
  }

  //Get the menu:
  Glib::RefPtr<Glib::Object> object =
    builder->get_object("UtilMenu");
  Glib::RefPtr<Gio::Menu> gmenu =
    Glib::RefPtr<Gio::Menu>::cast_dynamic(object);
  if(!gmenu)
    g_warning("GMenu not found");

  m_pPopupMenuUtils = new Gtk::Menu(gmenu);
#endif
}

void LayoutWidgetUtils::on_menu_delete_activate()
{
  Gtk::Widget* parent = dynamic_cast<Gtk::Widget*>(this);
  if(!parent)
  {
    // Should never happen!
    std::cerr << "LayoutWidgetUtils is no Gtk::Widget" << std::endl;
    return;
  }

  LayoutWidgetBase* base = 0;
  do
  {
    parent = parent->get_parent();
    base = dynamic_cast<LayoutWidgetBase*>(parent);
    if(base)
    {
      break;
    }
  } while (parent);

  if(base)
  {
    sharedptr<LayoutGroup> group = 
      sharedptr<LayoutGroup>::cast_dynamic(base->get_layout_item());
    if(!group)
      return;

    group->remove_item(get_layout_item());
    signal_layout_changed().emit();
  }
}

void LayoutWidgetUtils::on_menu_properties_activate()
{
  //This is not pure virtual, so we can easily use this base class in unit tests.
  std::cerr << G_STRFUNC << ": Not imlemented. Derived classes should override this." << std::endl;
}

#endif // !GLOM_ENABLE_CLIENT_ONLY

} // namespace Glom

