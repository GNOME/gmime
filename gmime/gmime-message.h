/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */
/*  GMime
 *  Copyright (C) 2000-2017 Jeffrey Stedfast
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation; either version 2.1
 *  of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free
 *  Software Foundation, 51 Franklin Street, Fifth Floor, Boston, MA
 *  02110-1301, USA.
 */


#ifndef __GMIME_MESSAGE_H__
#define __GMIME_MESSAGE_H__

#include <stdarg.h>
#include <time.h>

#include <gmime/internet-address.h>
#include <gmime/gmime-encodings.h>
#include <gmime/gmime-object.h>
#include <gmime/gmime-header.h>
#include <gmime/gmime-stream.h>

G_BEGIN_DECLS

#define GMIME_TYPE_MESSAGE            (g_mime_message_get_type ())
#define GMIME_MESSAGE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), GMIME_TYPE_MESSAGE, GMimeMessage))
#define GMIME_MESSAGE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), GMIME_TYPE_MESSAGE, GMimeMessageClass))
#define GMIME_IS_MESSAGE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), GMIME_TYPE_MESSAGE))
#define GMIME_IS_MESSAGE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), GMIME_TYPE_MESSAGE))
#define GMIME_MESSAGE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), GMIME_TYPE_MESSAGE, GMimeMessageClass))

typedef struct _GMimeMessage GMimeMessage;
typedef struct _GMimeMessageClass GMimeMessageClass;


/**
 * GMimeAddressType:
 * @GMIME_ADDRESS_TYPE_SENDER: Represents the addresses in the Sender header.
 * @GMIME_ADDRESS_TYPE_FROM: Represents the addresses in the From header.
 * @GMIME_ADDRESS_TYPE_REPLY_TO: Represents the addresses in the Reply-To header.
 * @GMIME_ADDRESS_TYPE_TO: Represents the recipients in the To header.
 * @GMIME_ADDRESS_TYPE_CC: Represents the recipients in the Cc header.
 * @GMIME_ADDRESS_TYPE_BCC: Represents the recipients in the Bcc header.
 *
 * An address type.
 **/
typedef enum _GMimeAddressType {
	GMIME_ADDRESS_TYPE_SENDER,
	GMIME_ADDRESS_TYPE_FROM,
	GMIME_ADDRESS_TYPE_REPLY_TO,
	GMIME_ADDRESS_TYPE_TO,
	GMIME_ADDRESS_TYPE_CC,
	GMIME_ADDRESS_TYPE_BCC
} GMimeAddressType;


/**
 * GMimeMessage:
 * @parent_object: parent #GMimeObject
 * @mime_part: toplevel MIME part
 * @addrlists: a table of address lists
 * @message_id: Message-Id string
 * @subject: Subject string
 * @date: Date value
 * @tz_offset: timezone offset
 *
 * A MIME Message object.
 **/
struct _GMimeMessage {
	GMimeObject parent_object;
	
	InternetAddressList **addrlists;
	GMimeObject *mime_part;
	char *message_id;
	char *subject;
	
	time_t date;
	int tz_offset;
};

struct _GMimeMessageClass {
	GMimeObjectClass parent_class;
	
};


GType g_mime_message_get_type (void);

GMimeMessage *g_mime_message_new (gboolean pretty_headers);

InternetAddressList *g_mime_message_get_from (GMimeMessage *message);
InternetAddressList *g_mime_message_get_sender (GMimeMessage *message);
InternetAddressList *g_mime_message_get_reply_to (GMimeMessage *message);
InternetAddressList *g_mime_message_get_to (GMimeMessage *message);
InternetAddressList *g_mime_message_get_cc (GMimeMessage *message);
InternetAddressList *g_mime_message_get_bcc (GMimeMessage *message);

void g_mime_message_add_mailbox (GMimeMessage *message, GMimeAddressType type, const char *name, const char *addr);
InternetAddressList *g_mime_message_get_addresses (GMimeMessage *message, GMimeAddressType type);
InternetAddressList *g_mime_message_get_all_recipients (GMimeMessage *message);

void g_mime_message_set_subject (GMimeMessage *message, const char *subject, const char *charset);
const char *g_mime_message_get_subject (GMimeMessage *message);

void g_mime_message_set_date (GMimeMessage *message, time_t date, int tz_offset);
void g_mime_message_get_date (GMimeMessage *message, time_t *date, int *tz_offset);
void g_mime_message_set_date_as_string (GMimeMessage *message, const char *str);
char *g_mime_message_get_date_as_string (GMimeMessage *message);

void g_mime_message_set_message_id (GMimeMessage *message, const char *message_id);
const char *g_mime_message_get_message_id (GMimeMessage *message);

GMimeObject *g_mime_message_get_mime_part (GMimeMessage *message);
void g_mime_message_set_mime_part (GMimeMessage *message, GMimeObject *mime_part);

/* convenience functions */

void g_mime_message_foreach (GMimeMessage *message, GMimeObjectForeachFunc callback,
			     gpointer user_data);

GMimeObject *g_mime_message_get_body (GMimeMessage *message);

G_END_DECLS

#endif /* __GMIME_MESSAGE_H__ */
