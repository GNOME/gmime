/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*
 *  Authors: Jeffrey Stedfast <fejj@helixcode.com>
 *
 *  Copyright 2000 Helix Code, Inc. (www.helixcode.com)
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Street #330, Boston, MA 02111-1307, USA.
 *
 */

#include "gmime-parser.h"
#include "gmime-utils.h"
#include <config.h>
#include <string.h>
#include <ctype.h>

#define d(x)

enum {
	CONTENT_TYPE = 0,
	CONTENT_TRANSFER_ENCODING,
	CONTENT_DISPOSITION,
	CONTENT_DESCRIPTION,
	CONTENT_LOCATION,
	CONTENT_MD5,
	CONTENT_ID
};

static gchar *content_headers[] = {
	"Content-Type:",
	"Content-Transfer-Encoding:",
	"Content-Disposition:",
	"Content-Description:",
	"Content-Location:",
	"Content-Md5:",
	"Content-Id:",
	NULL
};

static void
header_unfold (gchar *header)
{
	/* strip all \n's and replace tabs with spaces - this should
           undo any header folding */
	gchar *src, *dst;
	
	for (src = dst = header; *src; src++) {
		if (*src != '\n')
			*dst++ = *src != '\t' ? *src : ' ';
	}
	*dst = '\0';
}

static int
content_header (const gchar *field)
{
	int i;
	
	for (i = 0; content_headers[i]; i++)
		if (!g_strncasecmp (field, content_headers[i], strlen (content_headers[i])))
			return i;
	
	return -1;
}

#ifndef HAVE_ISBLANK
#define isblank(c) ((c) == ' ' || (c) == '\t')
#endif /* HAVE_ISBLANK */

static gchar *
g_strstrbound (gchar *haystack, const gchar *needle, const gchar *end)
{
	gboolean matches = FALSE;
	gchar *ptr;
	guint nlen;
	
	nlen = strlen (needle);
	ptr = haystack;
	
	while (ptr + nlen <= end) {
		if (!strncmp (ptr, needle, nlen)) {
			matches = TRUE;
			break;
		}
		ptr++;
	}
	
	if (matches)
		return ptr;
	else
		return NULL;
}

/* we pass the length here so that we can avoid dup'ing in the caller
   as mime parts can be BIG (tm) */
/**
 * g_mime_parser_construct_part: Construct a GMimePart object
 * @in: raw MIME Part data
 * @inlen: raw MIME Part data length
 *
 * Returns a GMimePart object based on the data.
 **/
GMimePart *
g_mime_parser_construct_part (const gchar *in, guint inlen)
{
	GMimePart *mime_part;
	gchar *boundary = NULL, *end_boundary = NULL;
	gchar *inptr, *inend;
	gboolean is_multipart = FALSE;
	
	g_return_val_if_fail (in != NULL, NULL);
	g_return_val_if_fail (inlen != 0, NULL);
	
	inptr = (gchar *) in;
	inend = inptr + inlen;
	
	mime_part = g_mime_part_new ();
	
	/* parse out the headers */
	while (inptr < inend) {
		GMimeContentType *mime_type;
		gchar *desc, *type, *encoding, *location;
		gchar *disp, *disposition, *id, *md5;
		gchar *ptr, *eptr, *text;
		
		eptr = strchr (inptr, '\n');
		if (!eptr)
			eptr = inend;
		
		switch (content_header (inptr)) {
		case CONTENT_DESCRIPTION:
			desc = inptr + strlen ("Content-Description:");			
			for (eptr = desc; eptr < inend; eptr++)
				if (*eptr == '\n' && !isblank (*(eptr + 1)))
					break;
			
			text = g_strndup (desc, (gint) (eptr - desc));
			desc = g_mime_utils_8bit_header_decode (text);
			g_strstrip (desc);
			g_free (text);
			
			g_mime_part_set_content_description (mime_part, desc);
			g_free (desc);
			
			inptr = eptr;
			break;
		case CONTENT_LOCATION:
			location = inptr + strlen ("Content-location:");			
			for (eptr = location; eptr < inend; eptr++)
				if (*eptr == '\n' && !isblank (*(eptr + 1)))
					break;
			
			location = g_strndup (location, (gint) (eptr - location));
			g_strstrip (location);
			
			g_mime_part_set_content_location (mime_part, location);
			g_free (location);
			
			inptr = eptr;
			break;
		case CONTENT_MD5:
			md5 = inptr + strlen ("Content-Md5:");
			for (eptr = md5; eptr < inend; eptr++)
				if (*eptr == '\n' && !isblank (*(eptr + 1)))
					break;
			
			md5 = g_strndup (md5, (gint) (eptr - md5));
			g_strstrip (md5);
			
			g_mime_part_set_content_md5 (mime_part, md5);
			g_free (md5);
			
			inptr = eptr;
			break;
		case CONTENT_ID:
			id = inptr + strlen ("Content-Id:");
			for (eptr = id; eptr < inend; eptr++)
				if (*eptr == '\n' && !isblank (*(eptr + 1)))
					break;
			
			id = g_strndup (id, (gint) (eptr - id));
			g_strstrip (id);
			
			g_mime_part_set_content_id (mime_part, id);
			g_free (id);
			
			inptr = eptr;
			break;
		case CONTENT_TYPE:
			type = inptr + strlen ("Content-Type:");
			for (eptr = type; eptr < inend; eptr++)
				if (*eptr == '\n' && !isblank (*(eptr + 1)))
					break;
			
			type = g_strndup (type, (gint) (eptr - type));
			g_strstrip (type);
			
			mime_type = g_mime_content_type_new_from_string (type);
			g_free (type);
			
			is_multipart = g_mime_content_type_is_type (mime_type, "multipart", "*");
			if (is_multipart) {
				const gchar *bndry;
				
				bndry = g_mime_content_type_get_parameter (mime_type, "boundary");
				if (bndry) {
					/* create our temp boundary vars */
					boundary = g_strdup_printf ("\n--%s\n", bndry);
					end_boundary = g_strdup_printf ("\n--%s--\n", bndry);
				} else {
					g_warning ("Invalid MIME structure: boundary not found for multipart"
						   " - defaulting to text/plain.");
					/* lets continue onward as if this was not a multipart */
					g_mime_content_type_destroy (mime_type);
					mime_type = g_mime_content_type_new ("text", "plain");
					is_multipart = FALSE;
				}
			}
			
			g_mime_part_set_content_type (mime_part, mime_type);
			
			inptr = eptr;
			break;
		case CONTENT_TRANSFER_ENCODING:
			encoding = inptr + strlen ("Content-Transfer-Encoding:");
			for (eptr = encoding; eptr < inend && *eptr != '\n'; eptr++);
			
			encoding = g_strndup (encoding, (gint) (eptr - encoding));
			g_strstrip (encoding);
			
			g_mime_part_set_encoding (mime_part, g_mime_part_encoding_from_string (encoding));
			
			g_free (encoding);
			
			inptr = eptr;
			break;
		case CONTENT_DISPOSITION:
			disp = inptr + strlen ("Content-Disposition:");
			for (eptr = disp; eptr < inend; eptr++)
				if (*eptr == '\n' && !isblank (*(eptr + 1)))
					break;
			
			disposition = g_strndup (disp, (gint) (eptr - disp));
			
			for (ptr = disposition; *ptr && *ptr != ';'; ptr++);
			
			disp = g_strndup (disposition, (gint) (ptr - disposition));
			g_strstrip (disp);
			
			g_mime_part_set_content_disposition (mime_part, disp);
			g_free (disp);
			
			while (*ptr == ';') {
				/* looks like we've got some parameters */
				gchar *name, *value, *ch;
				
				/* get the param name */
				for (name = ptr + 1; *name && !isspace (*name); name++);
				for (ptr = name; *ptr && *ptr != '='; ptr++);
				name = g_strndup (name, (gint) (ptr - name));
				g_strstrip (name);
				
				/* convert param name to lowercase */
				for (ch = name; *ch; ch++)
					*ch = tolower (*ch);
				
				/* skip any whitespace */
				for (value = ptr + 1; *value && isspace (*value); value++);
				
				if (*value == '"') {
					/* value is in quotes */
					value++;
					for (ptr = value; *ptr; ptr++)
						if (*ptr == '"' && *(ptr - 1) != '\\')
							break;
					value = g_strndup (value, (gint) (ptr - value));
					g_strstrip (value);
					g_mime_utils_unquote_string (value);
					
					for ( ; *ptr && *ptr != ';'; ptr++);
				} else {
					/* value is not in quotes */
					for (ptr = value; *ptr && *ptr != ';'; ptr++);
					value = g_strndup (value, (gint) (ptr - value));
					g_strstrip (value);
				}
				
				g_mime_part_add_content_disposition_parameter (mime_part, name, value);
				g_free (name);
				g_free (value);
			}
			g_free (disposition);
			
			inptr = eptr;
			break;
		default:
			break;
		}
		
		/* jump to the next line */
		for ( ; inptr < inend && *inptr != '\n'; inptr++);
		
		/* check for end-of-header */
		if (*inptr == '\n' && *(inptr + 1) == '\n') {
			/* we've reached the end of the mime header */
			inptr++;
			break;
		}
		
		inptr++;  /* get past the '\n' */
	}
	
	if (is_multipart && boundary && end_boundary) {
		/* get all the subparts */
		GMimePart *subpart;
		gchar *part_begin, *part_end;
		
		part_begin = g_strstrbound (inptr, boundary, inend);
		while (part_begin < inend) {
			/* make sure we're not looking at the end boundary */
			if (!strncmp (part_begin, end_boundary, strlen (end_boundary)))
				break;
			
			/* find the end of this part */
			part_end = g_strstrbound (part_begin + strlen (boundary), boundary, inend);
			if (!part_end || part_end >= inend) {
				part_end = g_strstrbound (part_begin + strlen (boundary), end_boundary, inend);
				if (!part_end || part_end >= inend)
					part_end = inend;
			}
			
			/* get the subpart */
			subpart = g_mime_parser_construct_part (part_begin, (guint) (part_end - part_begin));
			g_mime_part_add_subpart (mime_part, subpart);
			
			/* the next part begins where the last one left off */
			part_begin = part_end;
		}
		
		/* free our temp boundary strings */
		g_free (boundary);
		g_free (end_boundary);
	} else {
		GMimePartEncodingType encoding;
		gchar *content;
		guint len = 0;
		
		/* from here to the end is the content */
		if (inptr < inend) {
			for (inptr++; inptr < inend && isspace (*inptr); inptr++);
			len = inend - inptr;
			content = inptr;
			
			/* trim off excess trailing \n's */
			inend = content + len;
			while (len > 2 && *inend == '\n' && *(inend - 1) == '\n') {
				inend--;
				len--;
			}
		}
		
		encoding = g_mime_part_get_encoding (mime_part);
		
		if (len > 0)
			g_mime_part_set_pre_encoded_content (mime_part, content, len, encoding);
	}
	
	return mime_part;
}

enum {
	HEADER_FROM = 0,
	HEADER_REPLY_TO,
	HEADER_TO,
	HEADER_CC,
	HEADER_BCC,
	HEADER_SUBJECT,
	HEADER_DATE,
	HEADER_MESSAGE_ID,
	HEADER_UNKNOWN
};

static char *fields[] = {
	"From:",
	"Reply-To:",
	"To:",
	"Cc:",
	"Bcc:",
	"Subject:",
	"Date:",
	"Message-Id:",
	NULL
};

static gboolean
special_header (const gchar *field)
{
	return (!g_strcasecmp (field, "MIME-Version:") || content_header (field) != -1);
}

static void
construct_headers (GMimeMessage *message, const gchar *headers, gint inlen, gboolean save_extra_headers)
{
	gchar *field, *value, *raw, *q;
	gchar *inptr, *inend;
	time_t date;
	int offset = 0;
	int i;
	
	inptr = (gchar *) headers;
	inend = inptr + inlen;
	
	for ( ; inptr < inend; inptr++) {
		for (i = 0; fields[i]; i++)
			if (!g_strncasecmp (fields[i], inptr, strlen (fields[i])))
				break;
		
		if (!fields[i]) {
			field = inptr;
			for (q = field; q < inend && *q != ':'; q++);
			field = g_strndup (field, (gint) (q - field + 1));
			g_strstrip (field);
		} else {
			field = g_strdup (fields[i]);
		}
		
		value = inptr + strlen (field);
		for (q = value; q < inend; q++)
			if (*q == '\n' && !isblank (*(q + 1)))
				break;
		
		value = g_strndup (value, (gint) (q - value));
		g_strstrip (value);
		header_unfold (value);
		
		switch (i) {
		case HEADER_FROM:
			raw = g_mime_utils_8bit_header_decode (value);
			g_mime_message_set_sender (message, raw);
			g_free (raw);
			break;
		case HEADER_REPLY_TO:
			raw = g_mime_utils_8bit_header_decode (value);
			g_mime_message_set_reply_to (message, raw);
			g_free (raw);
			break;
		case HEADER_TO:
			g_mime_message_add_recipients_from_string (message, GMIME_RECIPIENT_TYPE_TO, value);
			break;
		case HEADER_CC:
			g_mime_message_add_recipients_from_string (message, GMIME_RECIPIENT_TYPE_CC, value);
			break;
		case HEADER_BCC:
			g_mime_message_add_recipients_from_string (message, GMIME_RECIPIENT_TYPE_BCC, value);
			break;
		case HEADER_SUBJECT:
			raw = g_mime_utils_8bit_header_decode (value);
			g_mime_message_set_subject (message, raw);
			g_free (raw);
			break;
		case HEADER_DATE:
			date = g_mime_utils_header_decode_date (value, &offset);
			g_mime_message_set_date (message, date, offset);
			break;
		case HEADER_MESSAGE_ID:
			raw = g_mime_utils_8bit_header_decode (value);
			g_mime_message_set_message_id (message, raw);
			g_free (raw);
			break;
		case HEADER_UNKNOWN:
		default:
			if (save_extra_headers && !special_header (field)) {
				field[strlen (field) - 1] = '\0'; /* kill the ':' */
				g_mime_message_add_arbitrary_header (message, field, value);
			}
			break;
		}
		
		g_free (field);
		g_free (value);
		
		if (q >= inend)
			break;
		else
			inptr = q;
	}
}


/**
 * g_mime_parser_construct_message: Construct a GMimeMessage object
 * @data: an rfc0822 message
 * @save_extra_headers: if TRUE, then store the arbitrary headers
 *
 * Returns a GMimeMessage object based on the rfc0822 data.
 **/
GMimeMessage *
g_mime_parser_construct_message (const gchar *data, gboolean save_extra_headers)
{
	GMimeMessage *message;
	GMimePart *part;
	gchar *end;
	
	g_return_val_if_fail (data != NULL, NULL);
	
	end = strstr (data, "\n\n");
	g_return_val_if_fail (end != NULL, NULL);
	
	message = g_mime_message_new ();
	
	construct_headers (message, data, (gint) (end - data), save_extra_headers);
	
	part = g_mime_parser_construct_part (data, strlen (data));
	
	g_mime_message_set_mime_part (message, part);
	
	return message;
}
