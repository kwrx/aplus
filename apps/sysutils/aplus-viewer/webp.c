/*
 * Author:
 *      Antonino Natale <antonio.natale97@hotmail.com>
 *
 * Copyright (c) 2013-2019 Antonino Natale
 *
 *
 * This file is part of aplus.
 *
 * aplus is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * aplus is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with aplus.  If not, see <http://www.gnu.org/licenses/>.
 */

/**
 * @brief WebP pictures for mupdf, which reads every other format the viewer shows by itself.
 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include <mupdf/fitz.h>
#include <webp/decode.h>

#include "webp.h"


/**
 * @brief A WebP document: the one picture it holds, decoded when it is opened.
 */

typedef struct {

    fz_document super;

    fz_image* image;

} viewer_webp_document_t;


/**
 * @brief The one page of a WebP document, sharing its document's picture.
 */

typedef struct {

    fz_page super;

    fz_image* image;

} viewer_webp_page_t;


static const char* viewer_webp_extensions[] = {"webp", NULL};
static const char* viewer_webp_mimetypes[]  = {"image/webp", NULL};


bool viewer_webp_recognize(const unsigned char head[VIEWER_WEBP_HEAD]) {

    return memcmp(head, "RIFF", 4) == 0 && memcmp(head + 8, "WEBP", 4) == 0;
}


/**
 * @brief Decodes a WebP file into a picture, premultiplied the way mupdf draws one.
 *
 * @param ctx The mupdf context.
 * @param buffer The whole file.
 * @return The picture; throws when libwebp cannot decode the file, an animated one included.
 */

static fz_image* viewer_webp_decode(fz_context* ctx, fz_buffer* buffer) {

    unsigned char* data;

    const size_t size = fz_buffer_storage(ctx, buffer, &data);

    WebPDecoderConfig config;

    if (!WebPInitDecoderConfig(&config) || WebPGetFeatures(data, size, &config.input) != VP8_STATUS_OK) {
        fz_throw(ctx, FZ_ERROR_FORMAT, "not a WebP picture");
    }

    if (config.input.has_animation) {
        fz_throw(ctx, FZ_ERROR_UNSUPPORTED, "animated WebP pictures are not supported");
    }


    const int alpha = config.input.has_alpha ? 1 : 0;

    fz_pixmap* pixmap = fz_new_pixmap(ctx, fz_device_rgb(ctx), config.input.width, config.input.height, NULL, alpha);
    fz_image* image   = NULL;

    fz_var(image);

    fz_try(ctx) {

        config.output.colorspace         = alpha ? MODE_rgbA : MODE_RGB;
        config.output.is_external_memory = 1;
        config.output.u.RGBA.rgba        = fz_pixmap_samples(ctx, pixmap);
        config.output.u.RGBA.stride      = fz_pixmap_stride(ctx, pixmap);
        config.output.u.RGBA.size        = (size_t)fz_pixmap_stride(ctx, pixmap) * (size_t)config.input.height;

        if (WebPDecode(data, size, &config) != VP8_STATUS_OK) {
            fz_throw(ctx, FZ_ERROR_FORMAT, "cannot decode this WebP picture");
        }

        image = fz_new_image_from_pixmap(ctx, pixmap, NULL);
    }
    fz_always(ctx) {

        WebPFreeDecBuffer(&config.output);
        fz_drop_pixmap(ctx, pixmap);
    }
    fz_catch(ctx) {
        fz_rethrow(ctx);
    }

    return image;
}


/**
 * @brief Releases a WebP document's picture.
 *
 * @param ctx The mupdf context.
 * @param document The document.
 */

static void viewer_webp_drop_document(fz_context* ctx, fz_document* document) {

    fz_drop_image(ctx, ((viewer_webp_document_t*)document)->image);
}


/**
 * @brief Counts the pages of a WebP document, which is always one.
 *
 * @param ctx Unused.
 * @param document Unused.
 * @param chapter Unused.
 * @return 1.
 */

static int viewer_webp_count_pages(fz_context* ctx, fz_document* document, int chapter) {

    (void)ctx;
    (void)document;
    (void)chapter;

    return 1;
}


/**
 * @brief The size of the page, one point to a pixel of the picture.
 *
 * @param ctx Unused.
 * @param page The page.
 * @param box Unused, a picture having no boxes but its own.
 * @return The bounds, in points.
 */

static fz_rect viewer_webp_bound_page(fz_context* ctx, fz_page* page, fz_box_type box) {

    (void)ctx;
    (void)box;

    const fz_image* image = ((viewer_webp_page_t*)page)->image;

    return fz_make_rect(0.0f, 0.0f, (float)image->w, (float)image->h);
}


/**
 * @brief Draws the picture over the whole page.
 *
 * @param ctx The mupdf context.
 * @param page The page.
 * @param device The device to draw on.
 * @param transform Where the page goes on the device.
 * @param cookie Unused.
 */

static void viewer_webp_run_page(fz_context* ctx, fz_page* page, fz_device* device, fz_matrix transform, fz_cookie* cookie) {

    (void)cookie;

    fz_image* image = ((viewer_webp_page_t*)page)->image;

    fz_fill_image(ctx, device, image, fz_pre_scale(transform, (float)image->w, (float)image->h), 1.0f, fz_default_color_params);
}


/**
 * @brief Releases the page's hold on the picture.
 *
 * @param ctx The mupdf context.
 * @param page The page.
 */

static void viewer_webp_drop_page(fz_context* ctx, fz_page* page) {

    fz_drop_image(ctx, ((viewer_webp_page_t*)page)->image);
}


/**
 * @brief Loads the one page of a WebP document.
 *
 * @param ctx The mupdf context.
 * @param document The document.
 * @param chapter Unused.
 * @param number The page, which has to be 0.
 * @return The page.
 */

static fz_page* viewer_webp_load_page(fz_context* ctx, fz_document* document, int chapter, int number) {

    (void)chapter;

    if (number != 0) {
        fz_throw(ctx, FZ_ERROR_ARGUMENT, "invalid page number %d", number);
    }


    viewer_webp_page_t* page = fz_new_derived_page(ctx, viewer_webp_page_t, document);

    page->super.bound_page        = viewer_webp_bound_page;
    page->super.run_page_contents = viewer_webp_run_page;
    page->super.drop_page         = viewer_webp_drop_page;

    page->image = fz_keep_image(ctx, ((viewer_webp_document_t*)document)->image);

    return &page->super;
}


/**
 * @brief Opens a WebP file as a document, decoding its picture.
 *
 * @param ctx The mupdf context.
 * @param handler Unused.
 * @param stream The file.
 * @param accel Unused.
 * @param dir Unused.
 * @param state Unused.
 * @return The document; throws when the picture cannot be decoded.
 */

static fz_document* viewer_webp_open_document(fz_context* ctx, const fz_document_handler* handler, fz_stream* stream, fz_stream* accel, fz_archive* dir, void* state) {

    (void)handler;
    (void)accel;
    (void)dir;
    (void)state;

    viewer_webp_document_t* document = fz_new_derived_document(ctx, viewer_webp_document_t);

    document->super.drop_document = viewer_webp_drop_document;
    document->super.count_pages   = viewer_webp_count_pages;
    document->super.load_page     = viewer_webp_load_page;


    fz_buffer* buffer = NULL;

    fz_var(buffer);

    fz_try(ctx) {

        buffer          = fz_read_all(ctx, stream, 0);
        document->image = viewer_webp_decode(ctx, buffer);
    }
    fz_always(ctx) {
        fz_drop_buffer(ctx, buffer);
    }
    fz_catch(ctx) {

        fz_drop_document(ctx, &document->super);
        fz_rethrow(ctx);
    }

    return &document->super;
}


/**
 * @brief Tells mupdf how sure this handler is that a file is a WebP picture.
 *
 * @param ctx The mupdf context.
 * @param handler Unused.
 * @param stream The file, positioned at its start.
 * @param dir Unused.
 * @param state Receives no state.
 * @param free_state Receives no function to free it with.
 * @return 100 for a WebP header, 0 for anything else.
 */

static int viewer_webp_recognize_content(fz_context* ctx, const fz_document_handler* handler, fz_stream* stream, fz_archive* dir, void** state, fz_document_recognize_state_free_fn** free_state) {

    (void)handler;
    (void)dir;

    if (state) {
        *state = NULL;
    }

    if (free_state) {
        *free_state = NULL;
    }

    if (!stream) {
        return 0;
    }


    unsigned char head[VIEWER_WEBP_HEAD];

    return fz_read(ctx, stream, head, sizeof(head)) == sizeof(head) && viewer_webp_recognize(head) ? 100 : 0;
}


const fz_document_handler viewer_webp_handler = {
    .open              = viewer_webp_open_document,
    .extensions        = viewer_webp_extensions,
    .mimetypes         = viewer_webp_mimetypes,
    .recognize_content = viewer_webp_recognize_content,
};
