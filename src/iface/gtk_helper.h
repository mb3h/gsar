#ifndef GTK_HELPER_H_INCLUDED__
#define GTK_HELPER_H_INCLUDED__

struct gtk_helper_i {
	void (*paint_data) (gtk_helper_s *this_,
		  int x, int y
		, int width, int height
		, enum pixel_format pf
		, const void *data
		);
};

#endif // GTK_HELPER_H_INCLUDED__
