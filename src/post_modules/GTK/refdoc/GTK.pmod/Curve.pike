//!@code{ GTK.Curve()@}
//!@xml{<image src='../images/gtk_curve.png'/>@}
//!
//!
//!

inherit GTK.DrawingArea;

static GTK.Curve create( );
//!

array get_vector( int num_points );
//!

GTK.Curve reset( );
//!

GTK.Curve set_curve_type( int type );
//! One of @[CURVE_TYPE_LINEAR], @[CURVE_TYPE_FREE] and @[CURVE_TYPE_SPLINE];
//!
//!

GTK.Curve set_gamma( float gamma );
//!

GTK.Curve set_range( float min_x, float max_x, float min_y, float max_y );
//!

GTK.Curve set_vector( int nelems, array curve );
//!
