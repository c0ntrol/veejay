/* Gveejay Reloaded - graphical interface for VeeJay
 * 	     (C) 2002-2005 Niels Elburg <nelburg@looze.net> 
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <gtk/gtk.h>
#include <glib.h>

extern veejay_release_track(int id, int release_this);
extern void	veejay_bind_track( int id, int bind_this );


typedef struct
{
	int track_id;
	GtkWidget *view;
} track_view_t;

static	void	cell_data_func(
	GtkTreeViewColumn *col, GtkCellRenderer *renderer,
	GtkTreeModel *model, GtkTreeIter *iter,
	gpointer user_data)
{
	gint state;
	gtk_tree_model_get(model, iter, 1, &state, -1 );

	if(state==1)
		g_object_set(renderer, "active", TRUE, NULL );
	else
		g_object_set(renderer, "active", FALSE, NULL );
	g_object_set( renderer, "activatable", TRUE, NULL );
}

static	void	cell_toggled_callback( GtkCellRenderer *cell, gchar *path_string, gpointer user_data )
{
	track_view_t *v = (track_view_t*) user_data;
	GtkWidget *view = v->view;
	GtkTreeModel *model = gtk_tree_view_get_model(view);
	GtkTreePath *path = gtk_tree_path_new_from_string( path_string );
	GtkTreeIter iter;

	gtk_tree_model_get_iter( model, &iter, path );
	gchar *data = NULL;
	gtk_tree_model_get( model, &iter,0, &data, -1 );

	int id_data = -1; // invalid
	char junk[10];
	sscanf( data, "%s %d", junk,&id_data );
	if( gtk_cell_renderer_toggle_get_active( cell ) )
	{
	     veejay_release_track( v->track_id, id_data);
	}
	else
	{
	     veejay_bind_track( v->track_id, id_data );
	}
	g_free(data);
	gtk_tree_path_free( path );
}

extern int *sequence_get_track_status(void *priv );

void		update_track_view( int n_tracks, GtkWidget *widget, void *user_data )
{
	GtkTreeView *view = GTK_TREE_VIEW(widget);
	GtkTreeModel *model = gtk_tree_view_get_model( view );
	GtkListStore *store = GTK_LIST_STORE( model );
	g_object_ref( model );
	gtk_tree_view_set_model( GTK_TREE_VIEW( view ) , NULL );

	GtkTreeIter *iter;
	gboolean valid;
	
	int *tmp = sequence_get_track_status( user_data );
	int index = 0;
	valid = gtk_tree_model_get_iter_first( GTK_TREE_MODEL( store ), &iter );
	while(valid)
	{
		gtk_list_store_set( store, &iter, 1, tmp[index], -1 );
		index ++;
		valid = gtk_tree_model_iter_next( GTK_TREE_MODEL(store), &iter );
	}
	free(tmp);
	gtk_tree_view_set_model( GTK_TREE_VIEW( view ), model );	
	g_object_unref( model );

}

GtkWidget	*get_track_tree( void *data)
{
	track_view_t *t = (track_view_t*) data;
	return t->view;
}

void *create_track_view(int track_id, int ref_tracks, void *user_data)
{
	GtkCellRenderer *renderer, *wrenderer;
	GtkTreeModel *model;
	GtkWidget *view;
	view = gtk_tree_view_new();
	renderer = gtk_cell_renderer_text_new();
	wrenderer = gtk_cell_renderer_toggle_new();

	track_view_t *my_view = (track_view_t*) malloc(sizeof(track_view_t));
	memset(my_view,0,sizeof(track_view_t));

	gtk_cell_renderer_toggle_set_active( wrenderer , FALSE );

	gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW( view ),
			-1,
			"T",
			renderer,
			"text",
			0,
			NULL );
	gtk_tree_view_insert_column_with_attributes(
			GTK_TREE_VIEW( view ),
			-1,
			"-",
			wrenderer,
			"activatable",
			1,
			NULL);
		
	GtkWidget *col = gtk_tree_view_get_column( GTK_TREE_VIEW( view ), 0 );
	gtk_tree_view_column_set_fixed_width( GTK_TREE_VIEW_COLUMN( col ), 20 );

	col = gtk_tree_view_get_column( GTK_TREE_VIEW( view ), 1 );
	gtk_tree_view_column_set_clickable( GTK_TREE_VIEW_COLUMN( col ), TRUE );
	gtk_tree_view_column_set_fixed_width( GTK_TREE_VIEW_COLUMN( col ), 10 );

	gtk_tree_view_column_set_cell_data_func( col,
		wrenderer, cell_data_func, NULL,NULL);

	/* build model */
	GtkListStore *store;
	GtkTreeIter iter;

	store = gtk_list_store_new( 2, G_TYPE_STRING, G_TYPE_INT );
	int i;
	for( i = 0; i < ref_tracks ; i ++ )
	{
		char str[10];
		sprintf(str,"Track %d",i);
		gtk_list_store_append( store, &iter );
		gtk_list_store_set( store, &iter,
			0,  str,
			1,  0,
			-1);
	}		

	GtkTreeSelection *selection;
	selection = gtk_tree_view_get_selection( GTK_TREE_VIEW( view ) );
	gtk_tree_selection_set_mode( selection, GTK_SELECTION_NONE );
	my_view->track_id = track_id;
	my_view->view = view;

	model = GTK_TREE_MODEL( store );
	gtk_tree_view_set_model ( GTK_TREE_VIEW( view ), model );
	g_signal_connect( wrenderer, "toggled",
		(GCallback) cell_toggled_callback, (gpointer*) my_view);


	g_object_unref( model );

	return (void*) my_view;
}
