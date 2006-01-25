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

#include "translatable_item.h"
#include <glibmm/i18n.h>
#include <iostream>

Glib::ustring TranslatableItem::m_current_locale;
Glib::ustring TranslatableItem::m_original_locale;

TranslatableItem::TranslatableItem()
: m_translatable_item_type(TRANSLATABLE_TYPE_INVALID)
{
}

TranslatableItem::TranslatableItem(const TranslatableItem& src)
: m_translatable_item_type(src.m_translatable_item_type),
  m_name(src.m_name),
  m_title(src.m_title),
  m_map_translations(src.m_map_translations)
{
}

TranslatableItem::~TranslatableItem()
{
}

TranslatableItem& TranslatableItem::operator=(const TranslatableItem& src)
{
  m_name = src.m_name;
  m_title = src.m_title;
  m_translatable_item_type = src.m_translatable_item_type;
  m_map_translations = src.m_map_translations;

  return *this;
}

bool TranslatableItem::operator==(const TranslatableItem& src) const
{
  bool bResult = (m_name == src.m_name);
  bResult == bResult &&  (m_title == src.m_title);
  bResult == bResult &&  (m_map_translations == src.m_map_translations);
  bResult == bResult && (m_translatable_item_type == src.m_translatable_item_type);

  return bResult;
}

bool TranslatableItem::operator!=(const TranslatableItem& src) const
{
  return !(operator==(src));
}

void TranslatableItem::set_translation(const Glib::ustring& locale, const Glib::ustring& translation)
{
  if(translation.empty())
  {
    //Remove it from the map, to save space:
    type_map_locale_to_translations::iterator iterFind = m_map_translations.find(locale);
    if(iterFind != m_map_translations.end())
      m_map_translations.erase(iterFind);
  }
  else
    m_map_translations[locale] = translation;
}

Glib::ustring TranslatableItem::get_translation(const Glib::ustring& locale) const
{
  type_map_locale_to_translations::const_iterator iterFind = m_map_translations.find(locale);
  if(iterFind != m_map_translations.end())
    return iterFind->second;
  else
    return Glib::ustring();
}

const TranslatableItem::type_map_locale_to_translations& TranslatableItem::_get_translations_map() const
{
  return m_map_translations;
}

bool TranslatableItem::get_has_translations() const
{
  return !m_map_translations.empty();
}


Glib::ustring TranslatableItem::get_title() const
{
  if(get_current_locale_not_original()) //Aviud this code if we don't need translations.
  {
    const Glib::ustring translated_title = get_translation(get_current_locale());
    if(!translated_title.empty())
      return translated_title;
    else if(!m_title.empty())
      return m_title; //The original, if there is no translation.
    else if(m_map_translations.empty())
    {
      return Glib::ustring();
    }
    else
    {
      //return the first translation, if any.
      //This would be quite unusual.
      type_map_locale_to_translations::const_iterator iter = m_map_translations.begin();
      return iter->second;
    }
  }
  else
    return m_title;
}

Glib::ustring TranslatableItem::get_title(const Glib::ustring& locale) const
{
  return get_translation(locale);
}


Glib::ustring TranslatableItem::get_title_original() const
{
  return m_title;
}

void TranslatableItem::set_title(const Glib::ustring& title)
{
  if(get_current_locale_not_original()) //Aviud this code if we don't need translations.
  {
    const Glib::ustring the_locale = get_current_locale();
    if(the_locale.empty())
      set_title_original(title);
    else
      set_translation(the_locale, title);
  }
  else
    set_title_original(title);
}

void TranslatableItem::set_title(const Glib::ustring& locale, const Glib::ustring& title)
{
  set_translation(locale, title);
}

void TranslatableItem::set_title_original(const Glib::ustring& title)
{
  m_title = title;
}

Glib::ustring TranslatableItem::get_current_locale()
{
  if(m_current_locale.empty())
  {
    char* cLocale = setlocale(LC_ALL, NULL); //Passing NULL means query, instead of set.
    if(cLocale)
    {
      //std::cout << "TranslatableItem::get_current_locale(): locale=" << cLocale << std::endl;
      m_current_locale = cLocale;

      //Remove the part after _ or ., because we want, for instance, "en" instead of "en_US.UTF-8".
      //because we do not have a list of locales yet - only a list of languages.
      Glib::ustring::size_type pos = m_current_locale.find("_");
      if(pos != Glib::ustring::npos)
        m_current_locale = m_current_locale.substr(0, pos);

      pos = m_current_locale.find(".");
      if(pos != Glib::ustring::npos)
        m_current_locale = m_current_locale.substr(0, pos);
    }
    else
      m_current_locale = "C";
  }

  return m_current_locale;
}

void TranslatableItem::set_current_locale(const Glib::ustring& locale)
{
  if(locale.empty())
    return;

  m_current_locale = locale;
}

void TranslatableItem::set_original_locale(const Glib::ustring& locale)
{
  if(locale.empty())
    return;

  m_original_locale = locale;
}


Glib::ustring TranslatableItem::get_original_locale()
{
  if(m_original_locale.empty())
    m_original_locale = "en"; //"en_US.UTF-8";

  return m_original_locale; 
}

bool TranslatableItem::get_current_locale_not_original()
{
  if(m_original_locale.empty())
    get_original_locale();

  if(m_current_locale.empty())
    get_current_locale();

  return m_original_locale != m_current_locale;
}

TranslatableItem::enumTranslatableItemType TranslatableItem::get_translatable_item_type()
{
  return m_translatable_item_type;
}

Glib::ustring TranslatableItem::get_translatable_type_name(enumTranslatableItemType item_type)
{
  if(item_type == TRANSLATABLE_TYPE_FIELD)
    return _("Field");
  else if(item_type == TRANSLATABLE_TYPE_RELATIONSHIP)
    return _("Relationship");
  else if(item_type == TRANSLATABLE_TYPE_RELATIONSHIP)
    return _("Layout Item");
 else if(item_type == TRANSLATABLE_TYPE_REPORT)
    return _("Report");
 else if(item_type == TRANSLATABLE_TYPE_TABLE)
    return _("Table");
 else
    return _("Unknown");
}

void TranslatableItem::set_name(const Glib::ustring& name)
{
  m_name = name;
}

Glib::ustring TranslatableItem::get_name() const
{
  return m_name;
}

bool TranslatableItem::get_name_not_empty() const
{
  return !(get_name().empty());
}

Glib::ustring TranslatableItem::get_title_or_name() const
{
  const Glib::ustring title = get_title();
  if(title.empty())
    return get_name();
  else
    return title;
}

