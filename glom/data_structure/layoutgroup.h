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
 
#ifndef GLOM_DATASTRUCTURE_LAYOUTGROUP_H
#define GLOM_DATASTRUCTURE_LAYOUTGROUP_H

#include "layoutitem.h"
#include <map>

class LayoutGroup
{
public:

  LayoutGroup();
  LayoutGroup(const LayoutGroup& src);
  LayoutGroup& operator=(const LayoutGroup& src);

  bool has_field(const Glib::ustring& field_name) const;
  void add_item(const LayoutItem& item);
  
  Glib::ustring m_group_name;
  Glib::ustring m_title;
  guint m_sequence;

  typedef std::map<int, LayoutItem> type_map_items;
  type_map_items m_map_items; 
};

#endif //GLOM_DATASTRUCTURE_LAYOUTGROUP_H



