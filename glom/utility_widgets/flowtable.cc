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

#include "flowtable.h"
#include "layoutwidgetbase.h"
#include <iostream>
#include <gdkmm/window.h>
#include <glom/utils_ui.h>

namespace Glom
{

static void container_forall_callback(GtkWidget* widget_gobj, void* data)
{
  try
  {
    FlowTable::ForallSlot& slot = *static_cast<FlowTable::ForallSlot*>(data);
    Gtk::Widget *const widget = Glib::wrap(widget_gobj);

    g_return_if_fail(widget != 0);

    slot(*widget);
  }
  catch(...)
  {
    Glib::exception_handlers_invoke();
  }
}


FlowTable::FlowTableItem::FlowTableItem(Gtk::Widget* first, FlowTable* /* flowtable */)
: m_first(first),
  m_second(0),
  m_expand_first_full(false),
  m_expand_second(false),
  m_has_allocated_column(false),
  m_allocated_column(0)
{

}

FlowTable::FlowTableItem::FlowTableItem(Gtk::Widget* first, Gtk::Widget* second, FlowTable* /* flowtable */)
: m_first(first),
  m_second(second),
  m_expand_first_full(false),
  m_expand_second(false),
  m_has_allocated_column(false),
  m_allocated_column(0)
{

}


FlowTable::FlowTable()
:
  m_columns_allocated_changed(false),
  m_columns_count(1),
  m_column_padding(Utils::DEFAULT_SPACING_SMALL), //A sane default.
  m_row_padding(Utils::DEFAULT_SPACING_SMALL), //A sane default.
  m_design_mode(false)
{
  set_has_window(false);
  set_redraw_on_allocate(false);
}

FlowTable::~FlowTable()
{
  //Delete managed children.
  //(For some reason this is not always happening via Gtk::Container:
  /* Actualy, don't do this because we would then be double-deleting what the
   * Container base class already deleted. We'll have to really find out if/why some
   * things are not being deleted. murrayc.
  bool one_deleted = true;
  while(!m_children.empty() && one_deleted)
  {
    one_deleted = false;

    type_vecChildren::iterator iter = m_children.begin();
    FlowTableItem& item = *iter;

    //Delete the widgets:
    if(item.m_first || item.m_second)
    {
      if(item.m_first && item.m_first->is_managed_())
        delete item.m_first;

      if(item.m_second && item.m_second->is_managed_())
        delete item.m_second;

      m_children.erase(iter);
      one_deleted = true; //Make sure that we loop again.
    }
  }
  */
}

void FlowTable::set_design_mode(bool value)
{
  m_design_mode = value;

  queue_draw(); //because this changes how the widget would be drawn.
}

void FlowTable::add(Gtk::Widget& first, Gtk::Widget& second, bool expand_second)
{
  FlowTableItem item(&first, &second, this);

  item.m_expand_second = expand_second; //Expand to fill the width for all of the second item.
  m_children.push_back(item);
  item.m_first->set_parent(*this);
  item.m_second->set_parent(*this);
}

void FlowTable::add(Gtk::Widget& first, bool expand)
{
  FlowTableItem item(&first, this);
  item.m_expand_first_full = expand; //Expand to fill the width for first and second.
  m_children.push_back(item);
  item.m_first->set_parent(*this);
}

void FlowTable::insert_before(Gtk::Widget& first, Gtk::Widget& before, bool expand)
{
  FlowTableItem item(&first, this);
  item.m_expand_first_full = expand;
  insert_before(item, before);
}

void FlowTable::insert_before(Gtk::Widget& first, Gtk::Widget& second, Gtk::Widget& before, bool expand_second)
{
  FlowTableItem item(&first, &second, this);
  item.m_expand_second = expand_second;
  insert_before(item, before);
}

void FlowTable::insert_before(FlowTableItem& item, Gtk::Widget& before)
{
  bool found = false;
  std::vector<FlowTableItem>::iterator pos;
  for(pos = m_children.begin(); pos != m_children.end(); ++pos)
  {
    FlowTableItem* item = &(*pos);
    if(item->m_first)
    {
      if(item->m_first->gobj() == before.gobj())
      {
        found = true;
        break;
      }

      Gtk::Alignment* alignment = dynamic_cast<Gtk::Alignment*>(item->m_first);
      if(alignment && alignment->get_child()->gobj() == before.gobj())
      {
        found = true;
        break;
      }
    }

    if(item->m_second)
    {
      if(item->m_second->gobj() == before.gobj())
      {
        found = true;
        break;
      }
      Gtk::Alignment* alignment = dynamic_cast<Gtk::Alignment*>(item->m_second);
      if(alignment && alignment->get_child()->gobj() == before.gobj())
      {
        found = true;
        break;
      }
    }
  }

  gtk_widget_set_parent(GTK_WIDGET(item.m_first->gobj()), GTK_WIDGET(gobj()));
  if(item.m_second)
  {
    gtk_widget_set_parent(GTK_WIDGET(item.m_second->gobj()), GTK_WIDGET(gobj()));
  }

  if(pos == m_children.end())
    m_children.push_back(item);
  else
    m_children.insert(pos, item);
}

void FlowTable::set_columns_count(guint value)
{
  //Silently correct an invalid value:
  if(value == 0)
    value = 1;

  m_columns_count = value;
}

guint FlowTable::get_columns_count() const
{
  return m_columns_count;
}

void FlowTable::get_item_requested_width(const FlowTableItem& item, int& first, int& second) const
{
  //Initialize output paramters:
  first = 0;
  second = 0;

  Gtk::Widget* first_widget = item.m_first;
  Gtk::Widget* second_widget = item.m_second;

  if(child_is_visible(first_widget) || child_is_visible(second_widget))
  {
    if(child_is_visible(first_widget))
    {
      //Discover how much space this child needs:
      Gtk::Requisition child_requisition_request;
      child_requisition_request = first_widget->size_request();
      first = child_requisition_request.width;
    }

    if(child_is_visible(second_widget))
    {
      //Discover how much space this child needs:
      Gtk::Requisition child_requisition_request;
      child_requisition_request = second_widget->size_request();
      second = child_requisition_request.width;
    }
  }
}

int FlowTable::get_item_requested_height(const FlowTableItem& item) const
{
  int result = 0;

  Gtk::Widget* first = item.m_first;
  Gtk::Widget* second = item.m_second;
  int max_child_height = 0;
  if(child_is_visible(first) || child_is_visible(second))
  {
    //Look at both widgets and see which one has most height:
    if(child_is_visible(first))
    {
      // Ask the child how much space it needs:
      Gtk::Requisition child_requisition;
      child_requisition = first->size_request();

      max_child_height = child_requisition.height;
    }

    if(child_is_visible(second))
    {
      // Ask the child how much space it needs:
      Gtk::Requisition child_requisition;
      child_requisition = second->size_request();

      max_child_height = MAX(max_child_height, child_requisition.height);
    }

    //Use the largest height (they are next to each other, horizontally):
    result += max_child_height;
  }

  return result;
}

int FlowTable::get_column_height(guint start_widget, guint widget_count, int& total_width) const
{
  //init_db_details output parameter:
  total_width = 0;

  //Just add the heights together:
  int column_height = 0;
  int column_width_first = 0;
  int column_width_second = 0;
  int column_width_singles = 0;
  guint i = 0;
  for(i = start_widget; i < (start_widget+widget_count);  ++i)
  {
    const FlowTableItem& item = m_children[i];
    const int item_height = get_item_requested_height(item);

    int item_width_first = 0;
    int item_width_second = 0;
    get_item_requested_width(item, item_width_first, item_width_second);

    column_height += item_height;

    //Add the padding if it's not the first widget, and if one is visible:
    if( (i != start_widget) && item_height)
    {
      column_height += m_row_padding;
    }

    if(item_height) //If one was visible.
    {
      if(child_is_visible(item.m_second))
      {
        column_width_first = MAX(column_width_first, item_width_first);
        column_width_second = MAX(column_width_second, item_width_second);
      }
      else
      {
        //If there is only a first widget and no second one, then that first widget should take up the whole width,
        //instead of increasing the width for the first sub-column:
        column_width_singles = MAX(column_width_singles, item_width_first);
      }
    }

  }

   total_width = column_width_first +  column_width_second;

   //See whether the single items need even more space:
   total_width = MAX(total_width, column_width_singles);

   if( (column_width_first > 0) && (column_width_second > 0) ) //Add padding if necessary.
     total_width += m_column_padding;

  return column_height;
}

int FlowTable::get_minimum_column_height(guint start_widget, guint columns_count, int& total_width) const
{
  //init_db_details output parameter:
  total_width = 0;

  //Discover how best (least column height) to arrange these widgets in these columns, keeping them in sequence,
  //and then say how high the columns must best.

  if(columns_count == 1)
  {
    //Just add the heights together:
    const guint widgets_count = m_children.size() - start_widget;
    int column_height = get_column_height(start_widget, widgets_count, total_width);

    return column_height;
  }
  else
  {
    //Try each combination of widgets in the first column, combined with the the other combinations in the following columns:
    int minimum_column_height = 0;
    int width_for_minimum_column_height = 0;
    bool at_least_one_combination_checked = false;

    const guint count_items_remaining = m_children.size() - start_widget;

    for(guint first_column_widgets_count = 1;  first_column_widgets_count <= count_items_remaining; ++first_column_widgets_count)
    {
      int first_column_width = 0;
      int first_column_height = get_column_height(start_widget, first_column_widgets_count, first_column_width);
      int minimum_column_height_sofar = first_column_height;

      int others_column_width = 0;
      const guint others_column_start_widget =  start_widget + first_column_widgets_count;
      //Call this function recursively to get the minimum column height in the other columns, when these widgets are in the first column:
      int minimum_column_height_nextcolumns = 0;
      if( others_column_start_widget  < m_children.size())
      {
        minimum_column_height_nextcolumns = get_minimum_column_height(others_column_start_widget, columns_count - 1, others_column_width);
        others_column_width += m_column_padding; //Add the padding between the previous column and this one.

        minimum_column_height_sofar = MAX(first_column_height, minimum_column_height_nextcolumns);
      }

      //See whether this is better than the last one:
      if(at_least_one_combination_checked)
      {
        if(minimum_column_height_sofar < minimum_column_height)
        {
          minimum_column_height = minimum_column_height_sofar;
          width_for_minimum_column_height = first_column_width + others_column_width;
        }
      }
      else
      {
        minimum_column_height = minimum_column_height_sofar;
        width_for_minimum_column_height = first_column_width + others_column_width;

        at_least_one_combination_checked = true;
      }
    }

    total_width = width_for_minimum_column_height;

    return minimum_column_height;
  }
}

void FlowTable::on_size_request(Gtk::Requisition* requisition)
{
  // Set a minimum size so people are able to drag items into the
  // table
  const int MIN_HEIGHT = (m_design_mode ? 50 : 0);
  const int MIN_WIDTH = (m_design_mode ? 100 : 0);

  //Initialize the output parameter:
  *requisition = Gtk::Requisition();

  //Discover the total amount of minimum space needed by this container widget, by examining its child widgets,
  //by examing every possible sequential arrangement of the widgets in this fixed number of columsn:
  int total_width = 0;
  const int column_height = get_minimum_column_height(0, m_columns_count, total_width); //This calls itself recursively.

  requisition->height = (column_height > MIN_HEIGHT ? column_height : MIN_HEIGHT);
  requisition->width = (total_width > MIN_WIDTH ? total_width : MIN_WIDTH);
}

//Give it whatever height/width it wants, at this location:
Gtk::Allocation FlowTable::assign_child(Gtk::Widget* widget, int x, int y, int width, int height)
{
  //Assign sign space to the child:
  Gtk::Allocation child_allocation;

  child_allocation.set_x(x);
  child_allocation.set_y(y);

  child_allocation.set_width(width);
  child_allocation.set_height(height);

  //g_warning("  assigning: x=%d, y=%d, width=%d, height=%d", x, y, child_requisition_request.width, child_requisition_request.height);
  //g_warning("    far x=%d, far y=%d", x+child_requisition_request.width , y+child_requisition_request.height);

  widget->size_allocate(child_allocation);

  return child_allocation; //Return this so we can cache it.
}

//Give it whatever height/width it wants, at this location:
Gtk::Allocation FlowTable::assign_child(Gtk::Widget* widget, int x, int y)
{
  //Discover how much space this child needs:
  Gtk::Requisition child_requisition_request;
  child_requisition_request = widget->size_request();

  //Give it as much space as it wants:
  return assign_child(widget, x, y, child_requisition_request.width, child_requisition_request.height);
}

void FlowTable::get_item_max_width_requested(guint start, guint height, guint& first_max_width, guint& second_max_width, guint& singles_max_width, bool& is_last_column) const
{
  //Initialize output parameters:
  first_max_width = 0;
  second_max_width = 0;
  singles_max_width = 0;
  is_last_column = false;

  if(m_children.empty())
    return;

  guint height_so_far = 0;
  bool first_item_added = false;
  guint i = start;
  while( (height_so_far < height) && (i < m_children.size()))
  {
    const FlowTableItem& item = m_children[i];
    Gtk::Widget* first = item.m_first;
    Gtk::Widget* second = item.m_second;

    guint height_item = 0;

    guint padding_above = 0;
    //Add padding above the item, if it is after another one.
    if( first_item_added && (child_is_visible(first) || child_is_visible(second)) )
       padding_above += m_row_padding;

    guint item_first_width = 0;
    guint singles_width = 0;
    if(child_is_visible(first))
    {
      Gtk::Requisition child_requisition;
      child_requisition = first->size_request();

      if(child_is_visible(second))
      {
        //There are 2 widgets so put one in each sub-column, lined up with the widgets in the other rows:
        item_first_width = child_requisition.width;
      }
      else
      {
        //There is only one widget, so let it take up the whole space and not affect the widths of the sub-columns:
        singles_width = child_requisition.width;
      }

      height_item = MAX(height_item, (guint)child_requisition.height);
      first_item_added = true;
    }

    guint item_second_width = 0;
    if(child_is_visible(second))
    {
      Gtk::Requisition child_requisition;
      child_requisition = second->size_request();

      item_second_width = child_requisition.width;
      //g_warning("item_second_width=%d", item_second_width);
      height_item = MAX(height_item, (guint)child_requisition.height);
      first_item_added = true;
    }

    if(height_item)
      height_item += padding_above;

    height_so_far += height_item;
    if(height_so_far <= height) //Don't remember the width details if the widgets are too high for the end of this column:
    {
      first_max_width = MAX(first_max_width, item_first_width);
      second_max_width = MAX(second_max_width, item_second_width);
      //g_warning("max item_second_width=%d", second_max_width);
      singles_max_width = MAX(singles_max_width, singles_width);
    }

    ++i;
  }

  //Tell the caller if this was the last column,
  //so that it can decide to use all available space.
  if(i == m_children.size())
    is_last_column = true;

  //g_warning("i=%d", i);
  //g_warning("get_item_max_width_requested(guint start=%d, guint height=%d, guint& first_max_width=%d, guint& second_max_width=%d",  start, height, first_max_width, second_max_width);
}

void FlowTable::on_size_allocate(Gtk::Allocation& allocation)
{
  //Do something with the space that we have actually been given:
  //(We will not be given heights or widths less than we have requested, though we might get more)

  set_allocation(allocation);

  //This will be used in on_expose_event():
  m_lines_horizontal.clear();
  m_lines_vertical.clear();

  //Discover the widths of the different parts of the first column:
  guint first_max_width = 0;
  guint second_max_width = 0;
  guint singles_max_width = 0;
  bool is_last_column = false;
  get_item_max_width_requested(0, allocation.get_height(), first_max_width, second_max_width, singles_max_width, is_last_column);  //TODO: Give the 2nd part of the column a bit more if the total needed is less than the allocation given.

  //Calculate where the columns should start on the x axis.
  int column_x_start = allocation.get_x();

  int column_x_start_second = column_x_start + first_max_width;
  if(first_max_width > 0) //Add padding between first and second sub sets of items, if there is a first set.
    column_x_start_second += m_column_padding;

  //Used for drawing horizontal lines:
  guint column_max_width = MAX(first_max_width + m_column_padding + second_max_width, singles_max_width);
  //Use the whole remaining width if there is no column after this:
  if(is_last_column)
  {
    column_max_width = allocation.get_width() - (column_x_start - allocation.get_x());
  }

  int column_child_y_start = allocation.get_y();

  guint column_number = 0; //Just for remembering it for later.
  guint count = m_children.size();
  for(guint i = 0; i < count; ++i)
  {
    FlowTableItem& item = m_children[i];

    const int item_height = get_item_requested_height(item);

    //Start a new column if necessary:
    int bottom = allocation.get_y() + allocation.get_height();
    if( (column_child_y_start + item_height) > bottom)
    {
      //start a new column:
      column_child_y_start = allocation.get_y();
      ++column_number;
      const int column_x_start_plus_singles = column_x_start + singles_max_width;
      column_x_start = column_x_start_second + second_max_width;
      column_x_start = MAX(column_x_start, column_x_start_plus_singles); //Maybe the single items take up even more width.
      column_x_start += m_column_padding;

      //Draw vertical line to separate the columns, in the middle of the padding:
      const int line_x = column_x_start - (m_column_padding / 2);
      const int line_height = allocation.get_height();
      m_lines_vertical.push_back( type_line( Gdk::Point(line_x, allocation.get_y()), Gdk::Point(line_x, allocation.get_y() + line_height) ) );

      {
        //Discover the widths of the different parts of this column:
        first_max_width = 0;
        second_max_width= 0;
        singles_max_width = 0;
        bool is_last_column = false;

        get_item_max_width_requested(i, allocation.get_height(), first_max_width, second_max_width, singles_max_width, is_last_column);

        column_x_start_second = column_x_start + first_max_width;
        if(first_max_width > 0) //Add padding between first and second sub sets of items, if there is a first set.
          column_x_start_second += m_column_padding;

        //Used for drawing horizontal lines:
        column_max_width = MAX(first_max_width + m_column_padding + second_max_width, singles_max_width);

        //Use the whole remaining width if there is no column after this:
        if(is_last_column)
        {
          column_max_width = allocation.get_width() - (column_x_start - allocation.get_x());
        }
      }
    }

    bool something_added = false;

    Gtk::Widget* first = item.m_first;
    Gtk::Widget* second = item.m_second;
    if(child_is_visible(second))
    {
      //Place the 2 items so that they line up with the 2 items in other rows:
      if(child_is_visible(first))
      {
        //Assign space to the child:

        //Make all the left edges line up, and give the full first-item width, even if that's more width than it requested,
        //so that it can align itself in the available space.
        item.m_first_allocation = assign_child(first, column_x_start, column_child_y_start, first_max_width, item_height);
        something_added = true;
      }

      if(true) //Unecessary check: child_is_visible(second))
      {
        //Assign space to the child:

        //Make all the left edges line up:

        if(!(item.m_expand_second))
          item.m_second_allocation = assign_child(second, column_x_start_second, column_child_y_start);
        else
        {
          const int second_width_available = column_max_width - first_max_width - m_column_padding;
          item.m_second_allocation = assign_child(second, column_x_start_second, column_child_y_start, second_width_available, item_height);
        }

        something_added = true;
      }
    }
    else if(child_is_visible(first))
    {
      //There is only one item - so let it take up the whole width of the column, if requested:

      //Assign space to the child:
      if(!(item.m_expand_first_full))
        item.m_first_allocation = assign_child(first, column_x_start, column_child_y_start);
      else
      {
        item.m_first_allocation = assign_child(first, column_x_start, column_child_y_start, column_max_width, item_height);
      }

      something_added = true;
    }

    if(something_added)
    {
      //Let later code know where the widgets are, and whether the layout has
      //changed since m_columns_allocated_changed was last set to false.
      if(!item.m_has_allocated_column || (item.m_allocated_column != column_number))
      {
        item.m_allocated_column = column_number;
        item.m_has_allocated_column = true;
        m_columns_allocated_changed = true;
      }

      //Start the next child below this child, plus the padding
      column_child_y_start += item_height;
      column_child_y_start += m_row_padding; //Ignored if this is the last item - we will just start a new column when we find that column_child_y_start is too much.

      //Add horizontal line in the middle of the padding:
      const guint line_y = column_child_y_start - (m_row_padding / 2);
      const guint line_width = column_max_width;
      m_lines_horizontal.push_back( type_line( Gdk::Point(column_x_start, line_y), Gdk::Point(column_x_start + line_width, line_y) ) );
    }
  }
}

GType FlowTable::child_type_vfunc() const
{
  //TODO: What is this for?
  if(!m_children.empty())
    return GTK_TYPE_WIDGET;
  else
    return G_TYPE_NONE;
}


void FlowTable::on_add(Gtk::Widget* child)
{
  FlowTableItem item(child, 0);
  m_children.push_back(item);

  gtk_widget_set_parent(GTK_WIDGET(item.m_first->gobj()), GTK_WIDGET(gobj()));

  //This is protected, but should be public: child.set_parent(*this);

}



void FlowTable::on_remove(Gtk::Widget* child)
{
  if(child)
  {
    const bool visible = child_is_visible(child);

  //g_warning("FlowTable::on_remove");
    for(type_vecChildren::iterator iter = m_children.begin(); iter != m_children.end(); ++iter)
    {
      FlowTableItem& item = *iter;

      if(item.m_first == child)
      {
            //g_warning("FlowTable::on_remove unparenting first");
        gtk_widget_unparent(GTK_WIDGET(item.m_first->gobj())); //This is protected, but should be public: child.unparent();
        item.m_first = 0;

       if(visible)
          queue_resize();
      }

      if(item.m_second == child)
      {
        //g_warning("FlowTable::on_remove unparenting second");
        gtk_widget_unparent(GTK_WIDGET(item.m_second->gobj())); //This is protected, but should be public: child.unparent();
        item.m_second = 0;

        if(visible)
          queue_resize();
      }
    }
  }

 //g_warning("FlowTable::on_remove end");
}


void FlowTable::forall_vfunc(gboolean /* include_internals */, GtkCallback callback, gpointer callback_data)
{
  for(type_vecChildren::const_iterator iter = m_children.begin(); iter != m_children.end(); ++iter)
  {
    const FlowTableItem& item = *iter;

    Gtk::Widget* first = item.m_first;
    if(first)
      callback(first->gobj(), callback_data);

    Gtk::Widget* second = item.m_second;
    if(second)
      callback(second->gobj(), callback_data);
  }
}

void FlowTable::forall(const ForallSlot& slot)
{
  ForallSlot slot_copy(slot);
  gtk_container_forall(gobj(), &container_forall_callback, &slot_copy);
}

void FlowTable::set_column_padding(guint padding)
{
  m_column_padding = padding;
}

guint FlowTable::get_column_padding() const
{
  return m_column_padding;
}

void FlowTable::set_row_padding(guint padding)
{
  m_row_padding = padding;
}

guint FlowTable::get_row_padding() const
{
  return m_row_padding;
}

bool FlowTable::child_is_visible(const Gtk::Widget* widget) const
{
  #if GTKMM_MINOR_VERSION >= 18
  return widget && widget->get_visible();
  #else
  return widget && widget->is_visible();
  #endif
}

void FlowTable::remove(Gtk::Widget& first)
{
  //Gtk::Container::remove() does this too. We need to do it here too:
  if(first.is_managed_())
    first.reference();

  gtk_widget_unparent(first.gobj());

  for(type_vecChildren::iterator iter = m_children.begin(); iter != m_children.end(); ++iter)
  {
    if((iter->m_first == &first) && (iter->m_second == 0))
    {
      //g_warning("FlowTable::remove(): removing %10X", (guint)&first);

      m_children.erase(iter);
      break;
    }
  }
}

void FlowTable::remove_all()
{

  for(type_vecChildren::iterator iter = m_children.begin(); iter != m_children.end(); ++iter)
  {
    if(iter->m_first)
    {
      Gtk::Widget* widget = iter->m_first;

      if(widget->is_managed_())
        widget->reference();

      gtk_widget_unparent(GTK_WIDGET(iter->m_first->gobj()));
    }

    if(iter->m_second)
    {
      Gtk::Widget* widget = iter->m_second;

      if(widget->is_managed_())
        widget->reference();

      gtk_widget_unparent(GTK_WIDGET(iter->m_second->gobj()));
    }

  }

  m_children.clear();
}

void FlowTable::on_realize()
{
  Gtk::Container::on_realize();

  if(!m_refGdkWindow)
  {
    m_refGdkWindow = get_window();
  }
}

void FlowTable::on_unrealize()
{
  m_refGdkWindow.reset();

  Gtk::Container::on_unrealize();
}

bool FlowTable::on_expose_event(GdkEventExpose* event)
{
  if(m_design_mode)
  {
    m_refGdkWindow = get_window();
    if(m_refGdkWindow)
    {
      Cairo::RefPtr<Cairo::Context> cr = m_refGdkWindow->create_cairo_context();
      cr->set_line_width(1);
      cr->set_line_cap(Cairo::LINE_CAP_SQUARE);
      cr->set_line_join(Cairo::LINE_JOIN_MITER);
      std::vector<double> dashes;
      dashes.push_back(10);
      cr->set_dash(dashes, 0);

      for(type_vecLines::iterator iter = m_lines_horizontal.begin(); iter != m_lines_horizontal.end(); ++iter)
      {
        cr->move_to(iter->first.get_x(), iter->first.get_y());
        cr->line_to(iter->second.get_x(), iter->second.get_y());
        cr->stroke();
      }

      for(type_vecLines::iterator iter = m_lines_vertical.begin(); iter != m_lines_vertical.end(); ++iter)
      {
        cr->move_to(iter->first.get_x(), iter->first.get_y());
        cr->line_to(iter->second.get_x(), iter->second.get_y());
        cr->stroke();
      }
    }
  }

  return Gtk::Container::on_expose_event(event);
}

bool FlowTable::get_column_for_first_widget(const Gtk::Widget& first, guint& column)
{
  //Initialize output parameter:
  column = 0;

  for(type_vecChildren::const_iterator iter = m_children.begin(); iter != m_children.end(); ++iter)
  {
    const FlowTableItem& item = *iter;

    if((&first == item.m_first) && item.m_has_allocated_column)
    {
       column = item.m_allocated_column;
       return true;
    }
  }

  return false;
}



} //namespace Glom
