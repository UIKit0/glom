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

#include <libglom/data_structure/report.h>

namespace Glom
{

Report::Report()
: m_show_table_title(true)
{
  m_translatable_item_type = TRANSLATABLE_TYPE_REPORT;
  m_layout_group = sharedptr<LayoutGroup>::create();
}

Report::Report(const Report& src)
: TranslatableItem(src),
  m_layout_group(src.m_layout_group),
  m_show_table_title(src.m_show_table_title)
{
}

Report& Report::operator=(const Report& src)
{
  TranslatableItem::operator=(src);

  m_layout_group = src.m_layout_group;
  m_show_table_title = src.m_show_table_title;

  return *this;
}

bool Report::get_show_table_title() const
{
  return m_show_table_title;
}

void Report::set_show_table_title(bool show_table_title)
{
  m_show_table_title = show_table_title;
}


sharedptr<LayoutGroup> Report::get_layout_group()
{
  return m_layout_group;
}

sharedptr<const LayoutGroup> Report::get_layout_group() const
{
  return m_layout_group;
}

} //namespace Glom

