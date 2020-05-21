#include <libsystem/assert.h>
#include <libwidget/Application.h>
#include <libwidget/Container.h>
#include <libwidget/Icon.h>
#include <libwidget/Panel.h>
#include <libwidget/Separator.h>

#include "paint/Canvas.h"
#include "paint/PaintDocument.h"
#include "paint/PaintTool.h"

static Color _color_palette[] = {
    COLOR(0x000000),
    COLOR(0x1a1c2c),
    COLOR(0x5d275d),
    COLOR(0xb13e53),
    COLOR(0xef7d57),
    COLOR(0xffcd75),
    COLOR(0xa7f070),
    COLOR(0x38b764),
    COLOR(0x257179),
    COLOR(0x29366f),
    COLOR(0x3b5dc9),
    COLOR(0x41a6f6),
    COLOR(0x73eff7),
    COLOR(0xffffff),
    COLOR(0xf4f4f4),
    COLOR(0x94b0c2),
    COLOR(0x566c86),
    COLOR(0x333c57),
};

typedef struct
{
    Window window;
    PaintDocument *document;

    /// --- Toolbar --- ///
    Widget *open_document;
    Widget *save_document;
    Widget *new_document;

    Widget *pencil;
    Widget *brush;
    Widget *eraser;
    Widget *fill;
    Widget *picker;

    // TODO:
    // Widget *insert_line;
    // Widget *insert_rectangle;
    // Widget *insert_circle;

    Widget *primary_color;
    Widget *secondary_color;

    /// --- Canvas --- ///
    Widget *canvas;

} PaintWindow;

static void update_toolbar(PaintWindow *window)
{
    widget_overwrite_color(window->primary_color, THEME_MIDDLEGROUND, window->document->primary_color);
    widget_overwrite_color(window->secondary_color, THEME_MIDDLEGROUND, window->document->secondary_color);
}

static void select_pencil(PaintWindow *window, ...)
{
    paint_document_set_tool(window->document, pencil_tool_create());

    update_toolbar(window);
}

static void select_brush(PaintWindow *window, ...)
{
    paint_document_set_tool(window->document, brush_tool_create());
    update_toolbar(window);
}

static void select_eraser(PaintWindow *window, ...)
{
    paint_document_set_tool(window->document, eraser_tool_create());
    update_toolbar(window);
}

static void select_fill(PaintWindow *window, ...)
{
    paint_document_set_tool(window->document, fill_tool_create());
    update_toolbar(window);
}

static void select_picker(PaintWindow *window, ...)
{
    paint_document_set_tool(window->document, picker_tool_create());
    update_toolbar(window);
}

static void create_toolbar(PaintWindow *window, Widget *parent)
{
    Widget *toolbar = panel_create(parent);

    toolbar->layout = HFLOW(12);
    toolbar->insets = INSETS(0, 8);

    window->open_document = icon_create(toolbar, "folder-open");
    window->save_document = icon_create(toolbar, "content-save");
    window->new_document = icon_create(toolbar, "image-plus");

    separator_create(toolbar);

    window->pencil = icon_create(toolbar, "pencil");
    widget_set_event_handler(window->pencil, EVENT_MOUSE_BUTTON_PRESS, window, (WidgetEventHandlerCallback)select_pencil);

    window->brush = icon_create(toolbar, "brush");
    widget_set_event_handler(window->brush, EVENT_MOUSE_BUTTON_PRESS, window, (WidgetEventHandlerCallback)select_brush);

    window->eraser = icon_create(toolbar, "eraser");
    widget_set_event_handler(window->eraser, EVENT_MOUSE_BUTTON_PRESS, window, (WidgetEventHandlerCallback)select_eraser);

    window->fill = icon_create(toolbar, "format-color-fill");
    widget_set_event_handler(window->fill, EVENT_MOUSE_BUTTON_PRESS, window, (WidgetEventHandlerCallback)select_fill);

    window->picker = icon_create(toolbar, "eyedropper");
    widget_set_event_handler(window->picker, EVENT_MOUSE_BUTTON_PRESS, window, (WidgetEventHandlerCallback)select_picker);

    separator_create(toolbar);

    // TODO:
    // window->insert_text = icon_create(toolbar, "format-text-variant");
    // window->insert_line = icon_create(toolbar, "vector-line");
    // window->insert_rectangle = icon_create(toolbar, "rectangle-outline");
    // window->insert_circle = icon_create(toolbar, "circle-outline");
    //
    // separator_create(toolbar);

    Widget *primary_color_container = container_create(toolbar);
    primary_color_container->insets = INSETS(8, 0);

    window->primary_color = panel_create(primary_color_container);
    widget_overwrite_color(window->primary_color, THEME_MIDDLEGROUND, window->document->primary_color);

    Widget *secondary_color_container = container_create(toolbar);
    secondary_color_container->insets = INSETS(8, 0);

    window->secondary_color = panel_create(secondary_color_container);
    widget_overwrite_color(window->secondary_color, THEME_MIDDLEGROUND, window->document->secondary_color);
}

static void on_color_palette_click(PaintWindow *window, Widget *sender, Event *event)
{
    if (event->mouse.button == MOUSE_BUTTON_LEFT)
    {
        window->document->primary_color = widget_get_color(sender, THEME_MIDDLEGROUND);
    }
    else if (event->mouse.button == MOUSE_BUTTON_RIGHT)
    {
        window->document->secondary_color = widget_get_color(sender, THEME_MIDDLEGROUND);
    }

    update_toolbar(window);
}

static void create_color_palette(PaintWindow *window, Widget *parent)
{
    __unused(window);

    Widget *palette = panel_create(parent);
    palette->layout = HGRID(1);

    for (size_t i = 0; i < 18; i++)
    {
        Widget *color_widget = panel_create(palette);
        widget_overwrite_color(color_widget, THEME_MIDDLEGROUND, _color_palette[i]);
        widget_set_event_handler(color_widget, EVENT_MOUSE_BUTTON_PRESS, window, (WidgetEventHandlerCallback)on_color_palette_click);
    }
}

static Window *paint_create_window(PaintDocument *document)
{
    PaintWindow *window = __create(PaintWindow);
    window->document = document;
    window_initialize((Window *)window, "brush", "Paint", 600, 560, WINDOW_RESIZABLE);

    Widget *root = window_root((Window *)window);
    root->layout = VFLOW(0);

    create_toolbar(window, root);

    window->canvas = canvas_create(root, window->document);
    window->canvas->layout_attributes = LAYOUT_FILL;

    create_color_palette(window, root);

    return (Window *)window;
}

int main(int argc, char **argv)
{
    application_initialize(argc, argv);

    PaintDocument *document = paint_document_create(400, 400, COLOR_RGBA(0, 0, 0, 0));

    Window *window = paint_create_window(document);
    window_show((Window *)window);

    int result = application_run();

    paint_document_destroy(document);

    return result;
}
