/* Glom
 *
 * Copyright (C) 2001-2011 Murray Cumming
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

#include "imageglom.h"
#include <glibmm/i18n.h>
#include <glom/appwindow.h>
#include <glom/utils_ui.h>
#include <glom/glade_utils.h>
#include <libglom/data_structure/glomconversions.h>
#include <glom/utility_widgets/dialog_image_load_progress.h>
#include <glom/utility_widgets/dialog_image_save_progress.h>
#include <gtkmm/appchooserdialog.h>
#include <gtkmm/filechooserdialog.h>
#include <giomm/file.h>
#include <giomm/contenttype.h>
#include <giomm/menu.h>
#include <libgda/gda-blob-op.h>
#include <glibmm/convert.h>

#ifdef G_OS_WIN32
#include <windows.h>
#endif

#include <iostream>   // for cout, endl

namespace Glom
{

ImageGlom::type_vec_ustrings ImageGlom::m_evince_supported_mime_types;
ImageGlom::type_vec_ustrings ImageGlom::m_gdkpixbuf_supported_mime_types;

ImageGlom::ImageGlom()
: m_ev_view(0),
  m_ev_document_model(0),
  m_pMenuPopup_UserMode(0)
{
  init();
}

ImageGlom::ImageGlom(BaseObjectType* cobject, const Glib::RefPtr<Gtk::Builder>& /* builder */)
: Gtk::EventBox(cobject),
  m_ev_view(0),
  m_ev_document_model(0),
  m_pMenuPopup_UserMode(0)
{
  init();
}

void ImageGlom::init()
{
  m_ev_view = EV_VIEW(ev_view_new());
  //gtk_widget_add_events(GTK_WIDGET(m_ev_view), GDK_BUTTON_PRESS_MASK);

  //Connect the the EvView's button-press-event signal, 
  //because we don't get it otherwise.
  //For some reason this is not necessary with the GtkImage.
  Gtk::Widget* cppEvView = Glib::wrap(GTK_WIDGET(m_ev_view));
  cppEvView->signal_button_press_event().connect(
    sigc::mem_fun(*this, &ImageGlom::on_button_press_event), false);

  m_read_only = false;

#ifndef GLOM_ENABLE_CLIENT_ONLY
  setup_menu(this);
#endif // !GLOM_ENABLE_CLIENT_ONLY

  setup_menu_usermode();

  //m_image.set_size_request(150, 150);


  m_frame.set_shadow_type(Gtk::SHADOW_ETCHED_IN); //Without this, the image widget has no borders and is completely invisible when empty.
  m_frame.show();

  add(m_frame);
}



ImageGlom::~ImageGlom()
{
  delete m_pMenuPopup_UserMode;
}

void ImageGlom::set_layout_item(const sharedptr<LayoutItem>& layout_item, const Glib::ustring& table_name)
{
  LayoutWidgetField::set_layout_item(layout_item, table_name);
#ifdef GTKMM_ATKMM_ENABLED
  get_accessible()->set_name(layout_item->get_name());
#endif  
}

bool ImageGlom::on_button_press_event(GdkEventButton *event)
{
  GdkModifierType mods;
  gdk_window_get_device_position( gtk_widget_get_window (Gtk::Widget::gobj()), event->device, 0, 0, &mods );

  //Enable/Disable items.
  //We did this earlier, but get_appwindow is more likely to work now:
  AppWindow* pApp = get_appwindow();
  if(pApp)
  {
#ifndef GLOM_ENABLE_CLIENT_ONLY
    pApp->add_developer_action(m_refContextLayout); //So that it can be disabled when not in developer mode.
    pApp->add_developer_action(m_refContextAddField);
    pApp->add_developer_action(m_refContextAddRelatedRecords);
    pApp->add_developer_action(m_refContextAddGroup);

    pApp->update_userlevel_ui(); //Update our action's sensitivity.
#endif // !GLOM_ENABLE_CLIENT_ONLY

    //Only show this popup in developer mode, so operators still see the default GtkEntry context menu.
    //TODO: It would be better to add it somehow to the standard context menu.
#ifndef GLOM_ENABLE_CLIENT_ONLY
    if(pApp->get_userlevel() == AppState::USERLEVEL_DEVELOPER)
    {
      if(mods & GDK_BUTTON3_MASK)
      {
        //Give user choices of actions on this item:
        popup_menu(event->button, event->time);
       
        return true; //We handled this event.
      }
    }
    else
#endif // !GLOM_ENABLE_CLIENT_ONLY
    {
      // We cannot be in developer mode in client only mode.
      if(mods & GDK_BUTTON3_MASK)
      {
        //Give user choices of actions on this item:
        popup_menu(event->button, event->time);

        return true; //We handled this event.
      }
    }

    //Single-click to select file:
    if(mods & GDK_BUTTON1_MASK)
    {
      on_menupopup_activate_select_file();
      return true; //We handled this event.

    }
  }

  return Gtk::EventBox::on_button_press_event(event);
}

AppWindow* ImageGlom::get_appwindow() const
{
  Gtk::Container* pWindow = const_cast<Gtk::Container*>(get_toplevel());
  //TODO: This only works when the child widget is already in its parent.

  return dynamic_cast<AppWindow*>(pWindow);
}

bool ImageGlom::get_has_original_data() const
{
  return true; //TODO.
}

/*
void ImageGlom::set_pixbuf(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf)
{
  m_pixbuf_original = pixbuf;
  show_image_data();
}
*/

void ImageGlom::set_value(const Gnome::Gda::Value& value)
{
  // Remember original data 
  m_original_data = Gnome::Gda::Value();
  m_original_data = value;
  show_image_data();
}

Gnome::Gda::Value ImageGlom::get_value() const
{
  return m_original_data;
}

void ImageGlom::on_size_allocate(Gtk::Allocation& allocation)
{
  Gtk::EventBox::on_size_allocate(allocation);

  //Resize the GtkImage if necessary:
  if(m_pixbuf_original)
  {
    const Glib::RefPtr<Gdk::Pixbuf> pixbuf_scaled = get_scaled_image();
    m_image.set(pixbuf_scaled);
  }
}

static void image_glom_ev_job_finished(EvJob* job, void* user_data)
{
  g_assert(job);
  
  ImageGlom* self = (ImageGlom*)user_data;
  g_assert(self);
  
  self->on_ev_job_finished(job);
}
  
void ImageGlom::on_ev_job_finished(EvJob* job)
{
  if(ev_job_is_failed (job)) {
    g_warning ("%s", job->error->message);
    g_object_unref (job);

    return;
  }

  ev_document_model_set_document(m_ev_document_model, job->document);
  ev_document_model_set_page(m_ev_document_model, 1);
  g_object_unref (job);
  
  //TODO: Show that we are no longer loading.
  //ev_view_set_loading(m_ev_view, FALSE);
}

const GdaBinary* ImageGlom::get_binary() const
{
  const GdaBinary* gda_binary = 0;
  if(m_original_data.get_value_type() == GDA_TYPE_BINARY)
    gda_binary = gda_value_get_binary(m_original_data.gobj());
  else if(m_original_data.get_value_type() == GDA_TYPE_BLOB)
  {
    const GdaBlob* gda_blob = gda_value_get_blob(m_original_data.gobj());
    if(gda_blob && gda_blob_op_read_all(const_cast<GdaBlobOp*>(gda_blob->op), const_cast<GdaBlob*>(gda_blob)))
      gda_binary = &(gda_blob->data);
  }
  
  return gda_binary;
}

Glib::ustring ImageGlom::get_mime_type() const
{
  const GdaBinary* gda_binary = get_binary();

  if(!gda_binary)
    return Glib::ustring();
    
  if(!gda_binary->data)
    return Glib::ustring();

  bool uncertain = false;
  const Glib::ustring result = Gio::content_type_guess(std::string(),
    gda_binary->data, gda_binary->binary_length,
    uncertain);

  //std::cout << G_STRFUNC << ": mime_type=" << result << ", uncertain=" << uncertain << std::endl;
  return result;  
}

void ImageGlom::fill_evince_supported_mime_types()
{
  //Fill the static list if it has not already been filled:
  if(!m_evince_supported_mime_types.empty())
    return;

  //Discover what mime types libevview can support.
  //Older versions supported image types too, via GdkPixbuf,
  //but that support was then removed.  
  GList* types_list = ev_backends_manager_get_all_types_info();
  if(!types_list)
  {
    return;
  }
  
  for(GList* l = types_list; l; l = g_list_next(l))
  {
    EvTypeInfo *info = (EvTypeInfo *)l->data;
    if(!info)
      continue;

    const char* mime_type = 0;
    int i = 0;
    while((mime_type = info->mime_types[i++]))
    {
      if(mime_type)
        m_evince_supported_mime_types.push_back(mime_type);
      //std::cout << "evince supported mime_type=" << mime_type << std::endl; 
    }
  }  
}

void ImageGlom::fill_gdkpixbuf_supported_mime_types()
{
  //Fill the static list if it has not already been filled:
  if(!m_gdkpixbuf_supported_mime_types.empty())
    return;
    
  typedef std::vector<Gdk::PixbufFormat> type_vec_formats;
  const type_vec_formats formats = Gdk::Pixbuf::get_formats();
  
  for(type_vec_formats::const_iterator iter = formats.begin();
    iter != formats.end(); ++iter)
  {
    const Gdk::PixbufFormat& format = *iter;
    const std::vector<Glib::ustring> mime_types = format.get_mime_types();
    m_gdkpixbuf_supported_mime_types.insert(
      m_gdkpixbuf_supported_mime_types.end(),
      mime_types.begin(), mime_types.end());
  }
}

void ImageGlom::show_image_data()
{
  bool use_evince = false;
  
  const Glib::ustring mime_type = get_mime_type();

  //std::cout << "mime_type=" << mime_type << std::endl; 
  
  fill_evince_supported_mime_types();
  const type_vec_ustrings::iterator iterFind = 
    std::find(m_evince_supported_mime_types.begin(),
      m_evince_supported_mime_types.end(),
      mime_type);
  if(iterFind != m_evince_supported_mime_types.end())
  {
    use_evince = true;
  }  
  
  m_frame.remove();
    
  //Clear all possible display widgets:
  m_pixbuf_original.reset();
  m_image.set(Glib::RefPtr<Gdk::Pixbuf>()); //TODO: Add an unset() to gtkmm.
  
  if(m_ev_document_model)
  {
    g_object_unref(m_ev_document_model);
    m_ev_document_model = 0;
  }

  if(use_evince)
  {
    //Use EvView:
    m_image.hide();
    
    gtk_widget_show(GTK_WIDGET(m_ev_view));
    gtk_container_add(GTK_CONTAINER(m_frame.gobj()), GTK_WIDGET(m_ev_view));

    // Try loading from data in memory:
    // TODO: Uncomment this if this API is added: https://bugzilla.gnome.org/show_bug.cgi?id=654832
    /*
    const GdaBinary* gda_binary = get_binary();
    if(!gda_binary || !gda_binary->data || !gda_binary->binary_length)
    {
       std::cerr << G_STRFUNC << "Data was null or empty." << std::endl;
      return;
    }
    
    EvJob *job = ev_job_load_new_with_data(
      (char*)gda_binary->data, gda_binary->binary_length);
    */
    //TODO: Test failure asynchronously.
    
    const Glib::ustring uri = save_to_temp_file(false /* don't show progress */);
    if(uri.empty())
    {
      std::cerr << G_STRFUNC << "Could not save temp file to show in the EvView." << std::endl;
    }
  
    EvJob *job = ev_job_load_new(uri.c_str());
  
    m_ev_document_model = ev_document_model_new();
    ev_view_set_model(m_ev_view, m_ev_document_model);
    ev_document_model_set_continuous(m_ev_document_model, FALSE); //Show only one page.

    //TODO: Show that we are loading.
    //ev_view_set_loading(m_ev_view, TRUE);

    g_signal_connect (job, "finished",
      G_CALLBACK (image_glom_ev_job_finished), this);
    ev_job_scheduler_push_job (job, EV_JOB_PRIORITY_NONE);
  }
  else
  {
    //Use GtkImage instead:
    gtk_widget_hide(GTK_WIDGET(m_ev_view));  
    m_image.show();
    m_frame.add(m_image);
    
    Glib::RefPtr<const Gio::Icon> icon;
      
    bool use_gdkpixbuf = false;
    fill_gdkpixbuf_supported_mime_types();
    const type_vec_ustrings::iterator iterFind = 
      std::find(m_gdkpixbuf_supported_mime_types.begin(),
        m_gdkpixbuf_supported_mime_types.end(),
        mime_type);
    if(iterFind != m_gdkpixbuf_supported_mime_types.end())
    {
      use_gdkpixbuf = true;
    }
    
    if(use_gdkpixbuf)
    {
      //Try to use GdkPixbuf's loader:
      m_pixbuf_original = UiUtils::get_pixbuf_for_gda_value(m_original_data);
    }
    else
    {
      //Get an icon for the file type;
      icon = Gio::content_type_get_icon(mime_type);
    }
    
    if(m_pixbuf_original)
    {
      Glib::RefPtr<Gdk::Pixbuf> pixbuf_scaled = get_scaled_image();
      m_image.set(pixbuf_scaled);
    }
    else if(icon)
    {
      m_image.set(icon, Gtk::ICON_SIZE_DIALOG);
    }
    else
    {
      m_image.set_from_icon_name("image-missing", Gtk::ICON_SIZE_DIALOG);
    }
  }
}

Glib::RefPtr<Gdk::Pixbuf> ImageGlom::get_scaled_image()
{
  Glib::RefPtr<Gdk::Pixbuf> pixbuf = m_pixbuf_original;

  if(!pixbuf)
    return pixbuf;
 
  const Gtk::Allocation allocation = m_image.get_allocation();
  const int pixbuf_height = pixbuf->get_height();
  const int pixbuf_width = pixbuf->get_width();
    
  const int allocation_height = allocation.get_height();
  const int allocation_width = allocation.get_width();
      
  //std::cout << "pixbuf_height=" << pixbuf_height << ", pixbuf_width=" << pixbuf_width << std::endl;
  //std::cout << "allocation_height=" << allocation.get_height() << ", allocation_width=" << allocation.get_width() << std::endl;

  if( (pixbuf_height > allocation_height) ||
      (pixbuf_width > allocation_width) )
  {
    if(true) //allocation_height > 10 || allocation_width > 10)
    {
      Glib::RefPtr<Gdk::Pixbuf> pixbuf_scaled = UiUtils::image_scale_keeping_ratio(pixbuf, allocation_height, allocation_width);
      
      //Don't set a new pixbuf if the dimensions have not changed:
      Glib::RefPtr<Gdk::Pixbuf> pixbuf_in_image;

      if(m_image.get_storage_type() == Gtk::IMAGE_PIXBUF) //Prevent warning.
        pixbuf_in_image = m_image.get_pixbuf();

      if( !pixbuf_in_image || !pixbuf_scaled || (pixbuf_in_image->get_height() != pixbuf_scaled->get_height()) || (pixbuf_in_image->get_width() != pixbuf_scaled->get_width()) )
      {
        /*
        std::cout << "get_scale(): returning scaled" << std::endl;
        if(pixbuf_scaled)
        {
          std::cout << "scaled height=" << pixbuf_scaled->get_height() << ", scaled width=" << pixbuf_scaled->get_width() << std::endl;
        }
        */
        
        return pixbuf_scaled;
      }
      else
      {
        //Return the existing one, 
        //instead of a new one with the same contents,
        //so no unnecessary changes will be triggered.
        return pixbuf_in_image;
      }
    }
  }
  
  //std::cout << "get_scaled(): returning original" << std::endl;
  return pixbuf;
}

void ImageGlom::on_menupopup_activate_open_file()
{
  open_with();
}

void ImageGlom::on_menupopup_activate_open_file_with()
{
  AppWindow* pApp = get_appwindow();

  //Offer the user a choice of suitable applications:
  const Glib::ustring mime_type = get_mime_type();
  if(mime_type.empty())
  {
    std::cerr << G_STRFUNC << ": mime_type is empty." << std::endl;
  }
  
  Gtk::AppChooserDialog dialog(mime_type);
  if(pApp)
    dialog.set_transient_for(*pApp);

  if(dialog.run() != Gtk::RESPONSE_OK)
    return;
  
  Glib::RefPtr<Gio::AppInfo> app_info = dialog.get_app_info();
  if(!app_info)
  {
    std::cerr << G_STRFUNC << ": app_info was null." << std::endl;
  }
  
  open_with(app_info);
}

static void make_file_read_only(const Glib::ustring& uri)
{
  std::string filepath;

  try
  {
    filepath = Glib::filename_from_uri(uri);
  }
  catch(const Glib::Error& ex)
  {
    std::cerr << G_STRFUNC << "Exception: " << ex.what() << std::endl;
    return;
  }
  
  if(filepath.empty())
  {
    std::cerr << G_STRFUNC << ": filepath is empty." << std::endl;
  }
  
  const int result = chmod(filepath.c_str(), S_IRUSR);
  if(result != 0)
  {
    std::cerr << G_STRFUNC << ": chmod() failed." << std::endl;
  }
  
  //Setting the attribute via gio gives us this exception:
  //"Setting attribute access::can-write not supported"
  /*
  Glib::RefPtr<Gio::File> file = Gio::File::create_for_uri(uri);

  Glib::RefPtr<Gio::FileInfo> file_info;

  try
  {
    file_info = file->query_info(G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
  }
  catch(const Glib::Error& ex)
  {
    std::cerr << G_STRFUNC << ": query_info() failed: " << ex.what() << std::endl;
    return;
  }
  
  if(!file_info)
  {
    std::cerr << G_STRFUNC << ": : file_info is null" << std::endl;
    return;
  }
  
  const bool can_write =
    file_info->get_attribute_boolean(G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
  if(!can_write)
    return;
    
  file_info->set_attribute_boolean(G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE, false);
  
  try
  {
    file->set_attributes_from_info(file_info);
  }
  catch(const Glib::Error& ex)
  {
    std::cerr << G_STRFUNC << ": set_attributes_from_info() failed: " << ex.what() << std::endl;
  }
  */
}

Glib::ustring ImageGlom::save_to_temp_file(bool show_progress)
{
  Glib::ustring uri = Utils::get_temp_file_uri("glom_image");
  if(uri.empty())
  {
    std::cerr << G_STRFUNC << ": : uri is empty." << std::endl;
  }
  
  bool saved = false;
  if(show_progress)
    saved = save_file(uri);
  else
    saved = save_file_sync(uri);
  
  if(!saved)
  {
    uri = Glib::ustring();
    std::cerr << G_STRFUNC << ": save_file() failed." << std::endl;
  }
  else
  {
    //Don't let people easily edit the saved file,
    //because they would lose data when it is automatically deleted later.
    //Also they might think that editing it will change it in the database.
    make_file_read_only(uri);
  }

  return uri;
}

void ImageGlom::open_with(const Glib::RefPtr<Gio::AppInfo>& app_info)
{
  const Glib::ustring uri = save_to_temp_file();
  if(uri.empty())
    return;

  if(app_info)
  {
    app_info->launch_uri(uri); //TODO: Get a GdkAppLaunchContext?
  }
  else
  {
    //TODO: Avoid duplication in xsl_utils.cc, by moving this into a utility function:  
#ifdef G_OS_WIN32
    // gtk_show_uri doesn't seem to work on Win32, at least not for local files
    // We use Windows API instead.
    // TODO: Check it again and file a bug if necessary.
    // TODO: and this might not be necessary with Gio::AppInfo::launch_default_for_uri().
    //   Previously we used gtk_show_uri().
    ShellExecute(0, "open", uri.c_str(), 0, 0, SW_SHOW);
#else
    Gio::AppInfo::launch_default_for_uri(uri);
#endif //G_OS_WIN32
  }
}


static void set_file_filter_images(Gtk::FileChooser& file_chooser)
{
  //Get image formats only:
  Glib::RefPtr<Gtk::FileFilter> filter = Gtk::FileFilter::create();
  filter->set_name(_("Images"));
  filter->add_pixbuf_formats();
  file_chooser.add_filter(filter);
  
  ev_document_factory_add_filters(GTK_WIDGET(file_chooser.gobj()), 0);
  
  //Make Images the currently-selected one:
  file_chooser.set_filter(filter);
  
  /*  ev_document_factory_add_filters() add this already:
  filter = Gtk::FileFilter::create();
  filter->set_name(_("All Files"));
  filter->add_pattern("*");
  file_chooser.add_filter(filter);
  */
}

void ImageGlom::on_menupopup_activate_save_file()
{
  AppWindow* pApp = get_appwindow();

  Gtk::FileChooserDialog dialog(_("Save Image"), Gtk::FILE_CHOOSER_ACTION_SAVE);
  if(pApp)
    dialog.set_transient_for(*pApp);
          
  set_file_filter_images(dialog);

  dialog.add_button(_("_Cancel"), Gtk::RESPONSE_CANCEL);
  dialog.add_button(_("_Save"), Gtk::RESPONSE_OK);
  const int response = dialog.run();
  dialog.hide();
  if(response != Gtk::RESPONSE_OK)
    return;
    
  const Glib::ustring uri = dialog.get_uri();
  if(uri.empty())
    return;
    
  save_file(uri);
}

bool ImageGlom::save_file_sync(const Glib::ustring& uri)
{
  //TODO: We should still do this asynchronously, 
  //even when we don't use the dialog's run() to do that
  //because we don't want to offer feedback.
  //Ideally, EvView would just load from data anyway.
  
  const GdaBinary* gda_binary = get_binary();
  if(!gda_binary)
  {
    std::cerr << G_STRFUNC << ": GdaBinary is null" << std::endl;
    return false;
  }
    
  if(!gda_binary->data)
  {
    std::cerr << G_STRFUNC << ": GdaBinary::data is null" << std::endl;
    return false;
  }

  try
  {
    const std::string filepath = Glib::filename_from_uri(uri);
    Glib::file_set_contents(filepath, (const char*)gda_binary->data, gda_binary->binary_length);
  }
  catch(const Glib::Error& ex)
  {
    std::cerr << G_STRFUNC << "Exception: " << ex.what() << std::endl;
    return false;
  }
  
  return true;
}

bool ImageGlom::save_file(const Glib::ustring& uri)
{
  DialogImageSaveProgress* dialog_save = 0;
  Utils::get_glade_widget_derived_with_warning(dialog_save);
  if(!dialog_save)
    return false;
    
  // Automatically delete the dialog when we no longer need it:
  std::auto_ptr<Gtk::Dialog> dialog_keeper(dialog_save);

  AppWindow* pApp = get_appwindow();
  if(pApp)
    dialog_save->set_transient_for(*pApp);

  const GdaBinary* gda_binary = get_binary();
  if(!gda_binary)
    return false;

  dialog_save->set_image_data(*gda_binary);
  dialog_save->save(uri);

  dialog_save->run();

  return true;
}

void ImageGlom::on_menupopup_activate_select_file()
{
  if(m_read_only)
    return;
    
  AppWindow* pApp = get_appwindow();

  Gtk::FileChooserDialog dialog(_("Choose Image"), Gtk::FILE_CHOOSER_ACTION_OPEN);
  if(pApp)
    dialog.set_transient_for(*pApp);
          
  set_file_filter_images(dialog);

  dialog.add_button(_("_Cancel"), Gtk::RESPONSE_CANCEL);
  dialog.add_button(_("Select"), Gtk::RESPONSE_OK);
  int response = dialog.run();
  dialog.hide();

  if((response != Gtk::RESPONSE_CANCEL) && (response != Gtk::RESPONSE_DELETE_EVENT))
  {
    const Glib::ustring uri = dialog.get_uri();
    if(!uri.empty())
    {
      DialogImageLoadProgress* dialog;
      Utils::get_glade_widget_derived_with_warning(dialog);
      if(dialog)
      {
        // Automatically delete the dialog when we no longer need it:
        std::auto_ptr<Gtk::Dialog> dialog_keeper(dialog);

        if(pApp)
          dialog->set_transient_for(*pApp);

        dialog->load(uri);

        if(dialog->run() == Gtk::RESPONSE_ACCEPT)
        {
          GdaBinary* bin = g_new(GdaBinary, 1);
          std::auto_ptr<GdaBinary> image_data = dialog->get_image_data();
          bin->data = image_data->data;
          bin->binary_length = image_data->binary_length;

          m_original_data = Gnome::Gda::Value();
          
          g_value_unset(m_original_data.gobj());
          g_value_init(m_original_data.gobj(), GDA_TYPE_BINARY);
          gda_value_take_binary(m_original_data.gobj(), bin);

          show_image_data();
          signal_edited().emit();
        }
      }
    }
  }
}

void ImageGlom::on_clipboard_get(Gtk::SelectionData& selection_data, guint /* info */)
{
  //info is meant to indicate the target, but it seems to be always 0,
  //so we use the selection_data's target instead.

  const std::string target = selection_data.get_target(); 

  const Glib::ustring mime_type = get_mime_type();
  if(mime_type.empty())
  {
    std::cerr << G_STRFUNC << ": mime_type is empty." << std::endl;
  }
  
  if(target == mime_type)
  {
    const GdaBinary* gda_binary = get_binary();
    if(!gda_binary)
      return;
    
    if(!gda_binary->data)
      return;
    
    selection_data.set(mime_type, 8, gda_binary->data, gda_binary->binary_length);

    // This set() override uses an 8-bit text format for the data.
    //selection_data.set_pixbuf(m_pixbuf_clipboard);
  }
  else
  {
    std::cout << "ExampleWindow::on_clipboard_get(): Unexpected clipboard target format. expected: " << mime_type << std::endl;
  }
}

void ImageGlom::on_clipboard_clear()
{
  if(m_read_only)
    return;

  m_pixbuf_clipboard.reset();
}

void ImageGlom::on_menupopup_activate_copy()
{
  if(m_pixbuf_original)
  {
    //When copy is used, store it here until it is pasted.
    m_pixbuf_clipboard = m_pixbuf_original->copy(); //TODO: Get it from the DB, when we stop storing the original here instead of just the preview.
  }
  else
    m_pixbuf_clipboard.reset();

  Glib::RefPtr<Gtk::Clipboard> refClipboard = Gtk::Clipboard::get();

  //Targets:
  const Glib::ustring mime_type = get_mime_type();
  if(mime_type.empty())
  {
    std::cerr << G_STRFUNC << ": mime_type is empty." << std::endl;
  }
  
  std::vector<Gtk::TargetEntry> listTargets;
  listTargets.push_back( Gtk::TargetEntry(mime_type) );

  refClipboard->set( listTargets, sigc::mem_fun(*this, &ImageGlom::on_clipboard_get), sigc::mem_fun(*this, &ImageGlom::on_clipboard_clear) );
}

void ImageGlom::on_clipboard_received_image(const Glib::RefPtr<Gdk::Pixbuf>& pixbuf)
{
  if(m_read_only)
    return;

  if(pixbuf)
  {
    // Clear original data of previous image
    m_original_data = Gnome::Gda::Value();

    m_pixbuf_original = pixbuf;
    show_image_data();

    signal_edited().emit();
  }
}


void ImageGlom::on_menupopup_activate_paste()
{
  if(m_read_only)
    return;

  //Tell the clipboard to call our method when it is ready:
  Glib::RefPtr<Gtk::Clipboard> refClipboard = Gtk::Clipboard::get();

  if(refClipboard)
    refClipboard->request_image( sigc::mem_fun(*this, &ImageGlom::on_clipboard_received_image) );
}

void ImageGlom::on_menupopup_activate_clear()
{
  if(m_read_only)
    return;

  m_original_data = Gnome::Gda::Value();
  show_image_data();
  signal_edited().emit();
}

void ImageGlom::setup_menu_usermode()
{
  //Create the Gio::ActionGroup and associate it with this widget:
  m_refActionGroup_UserModePopup = Gio::SimpleActionGroup::create();

  m_refActionOpenFile = m_refActionGroup_UserModePopup->add_action("open-file",
    sigc::mem_fun(*this, &ImageGlom::on_menupopup_activate_open_file) );

  m_refActionOpenFileWith = m_refActionGroup_UserModePopup->add_action("open-fil-ewith",
    sigc::mem_fun(*this, &ImageGlom::on_menupopup_activate_open_file_with) );
    
  m_refActionSaveFile = m_refActionGroup_UserModePopup->add_action("save-file",
    sigc::mem_fun(*this, &ImageGlom::on_menupopup_activate_save_file) );
    
  m_refActionSelectFile = m_refActionGroup_UserModePopup->add_action("select-file",
    sigc::mem_fun(*this, &ImageGlom::on_menupopup_activate_select_file) );

  m_refActionCopy = m_refActionGroup_UserModePopup->add_action("copy",
    sigc::mem_fun(*this, &ImageGlom::on_menupopup_activate_copy) );

  m_refActionPaste = m_refActionGroup_UserModePopup->add_action("paste",
    sigc::mem_fun(*this, &ImageGlom::on_menupopup_activate_paste) );

  m_refActionClear = m_refActionGroup_UserModePopup->add_action("clear",
    sigc::mem_fun(*this, &ImageGlom::on_menupopup_activate_clear) );

  insert_action_group("imagecontext", m_refActionGroup_UserModePopup);


  //Create the UI for the menu whose items will activate the actions,
  //when this UI (a GtkMenu) is added and shown:

  Glib::RefPtr<Gio::Menu> menu = Gio::Menu::create();
  menu->append(_("_Open File"), "context.open-file");
  menu->append(_("Open File With"), "context.open-file-with");
  menu->append(_("Select File"), "context.select-file");
  menu->append(_("_Copy"), "context.copy");
  menu->append(_("_Paste"), "context.paste");
  menu->append(_("_Clear"), "context.clear");

  m_pMenuPopup_UserMode = new Gtk::Menu(menu);
  m_pMenuPopup_UserMode->attach_to_widget(*this);
}

void ImageGlom::do_choose_image()
{
  on_menupopup_activate_select_file();
}

void ImageGlom::set_read_only(bool read_only)
{
  m_read_only = read_only;
}

void ImageGlom::popup_menu(guint button, guint32 activate_time)
{
  if(!m_pMenuPopup_UserMode)
  {
    std::cerr << G_STRFUNC << ": m_pMenuPopup_UserMode is null" << std::endl;
    return;
  }

  m_pMenuPopup_UserMode->popup(button, activate_time);

  m_refActionSelectFile->set_enabled();
}


} //namespace Glom
