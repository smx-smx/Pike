//!

inherit GTK.Vbox;

Gnome.IconSelection add_defaults( );
//! Adds the default pixmap directory into the selection widget.
//!
//!

Gnome.IconSelection add_directory( string dir );
//! Adds the icons from the directory dir to the selection widget.
//!
//!

Gnome.IconSelection clear( int|void not_shown );
//! Clear the currently shown icons, the ones that weren't shown yet
//! are not cleared unless the not_shown parameter is given, in which
//! case even those are cleared.
//!
//!

static Gnome.IconSelection create( );
//! reates a new icon selection widget, it uses a W(GnomeIconList) for
//! the listing of icons
//!
//!

string get_icon( int full_path );
//! Gets the currently selected icon name, if full_path is true, it
//! returns the full path to the icon, if none is selected it returns
//! 0.
//!
//!

Gnome.IconSelection select_icon( string filename );
//! Selects the icon filename. This icon must have already been added
//! and shown
//!
//!

Gnome.IconSelection show_icons( );
//! Shows the icons inside the widget that were added with add_defaults
//! and add_directory. Before this function isf called the icons aren't
//! actually added to the listing and can't be picked by the user.
//!
//!
