/*
 *  metaserver_dialogs.cpp - UI for metaserver client

	Copyright (C) 2004 and beyond by Woody Zenfell, III
	and the "Aleph One" developers.

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
	GNU General Public License for more details.

	This license is contained in the file "COPYING",
	which is included with this source code; it is available online at
	http://www.gnu.org/licenses/gpl.html

 April 29, 2004 (Woody Zenfell):
	Created.
 */

#include "cseries.h"

#include "metaserver_dialogs.h"

#include "network_private.h" // GAME_PORT
#include "sdl_dialogs.h"
#include "sdl_fonts.h"
#include "sdl_widgets.h"
#include "screen_drawing.h"
#include "network_dialog_widgets_sdl.h" // chat_history widget
#include "network_metaserver.h"
#include "preferences.h"


static IPaddress sJoinAddress;


template <typename tElement>
class w_items_in_room : public w_list_base
{
public:
	typedef typename std::vector<tElement> ElementVector;
	typedef const ElementVector (MetaserverClient::*CollectionAccessor)() const;
	typedef void (*ItemClicked)(const tElement& item, w_items_in_room<tElement>* sender);

	w_items_in_room(MetaserverClient& client, CollectionAccessor accessor, ItemClicked itemClicked, int width, int numRows) :
	w_list_base(width, numRows, 0),
	m_client(client),
	m_accessor(accessor),
	m_itemClicked(itemClicked)
	{
		num_items = 0;
		new_items();
	}

	void collection_changed()
	{
		m_items = (m_client.*(m_accessor))();
		num_items = m_items.size();
		new_items();
		// do other crap - manage selection, force redraw, etc.
	}

	void item_selected()
	{
		if(m_itemClicked != NULL)
			(*m_itemClicked)(m_items[selection], this);
	}

protected:
	void draw_items(SDL_Surface* s) const
	{
		typename ElementVector::const_iterator i = m_items.begin();
		int16 x = rect.x + get_dialog_space(LIST_L_SPACE);
		int16 y = rect.y + get_dialog_space(LIST_T_SPACE);
		uint16 width = rect.w - get_dialog_space(LIST_L_SPACE) - get_dialog_space(LIST_R_SPACE);
	
		for(size_t n = 0; n < top_item; n++)
			++i;
	
		for (size_t n=top_item; n<top_item + MIN(shown_items, num_items); n++, ++i, y=y+font_height)
			draw_item(*i, s, x, y, width, n == selection && active);
	}	

private:
	MetaserverClient&	m_client;
	CollectionAccessor	m_accessor;
	ElementVector		m_items;
	ItemClicked		m_itemClicked;

	void draw_item(const tElement& item, SDL_Surface* s,
		int16 x, int16 y, uint16 width, bool selected) const
	{
		y += font->get_ascent();
		set_drawing_clip_rectangle(0, x, static_cast<short>(s->h), x + width);
		draw_text(s, item.name().c_str(), x, y, selected ? get_dialog_color(ITEM_ACTIVE_COLOR) : get_dialog_color(ITEM_COLOR), font, style);
		set_drawing_clip_rectangle(SHRT_MIN, SHRT_MIN, SHRT_MAX, SHRT_MAX);
	}
	
};



typedef w_items_in_room<MetaserverPlayerInfo> w_players_in_room;
typedef w_items_in_room<GameListMessage::GameListEntry> w_games_in_room;



enum { iCHAT_HISTORY = 5342, iCHAT_ENTRY, iPLAYERS_IN_ROOM, iGAMES_IN_ROOM };

static MetaserverClient* sMetaserverClient = NULL;

class NotificationAdapter : public MetaserverClient::NotificationAdapter
{
public:
	NotificationAdapter(dialog& d) : m_dialog(d) {}

	void playersInRoomChanged()
	{
		itemsInRoomChanged<w_players_in_room>(iPLAYERS_IN_ROOM);
	}

	void gamesInRoomChanged()
	{
		itemsInRoomChanged<w_games_in_room>(iGAMES_IN_ROOM);
	}
	
	void receivedChatMessage(const std::string& senderName, uint16 senderID, const std::string& message)
	{
		w_chat_history* ch = dynamic_cast<w_chat_history*>(m_dialog.get_widget_by_id(iCHAT_HISTORY));
		assert(ch != NULL);

		ch->append_chat_entry(senderName.c_str(), 0xcccccccc, 0xcccccccc, message.c_str());

		m_dialog.draw_dirty_widgets();
	}

	void receivedBroadcastMessage(const std::string& message)
	{
		w_chat_history* ch = dynamic_cast<w_chat_history*>(m_dialog.get_widget_by_id(iCHAT_HISTORY));
		assert(ch != NULL);

		ch->append_chat_entry(NULL, message.c_str());

		m_dialog.draw_dirty_widgets();
	}

private:
	dialog&	m_dialog;

	template <typename tWidget>
	void itemsInRoomChanged(uint16 widgetID)
	{
		tWidget* w = dynamic_cast<tWidget*>(m_dialog.get_widget_by_id(widgetID));
		assert(w != NULL);

		w->collection_changed();

		m_dialog.draw_dirty_widgets();
	}	
};



static void
gameClicked(const GameListMessage::GameListEntry& entry, w_games_in_room* sender)
{
	memcpy(&sJoinAddress.host, &entry.m_ipAddress, sizeof(sJoinAddress.host));
	sJoinAddress.port = entry.m_port;
	sender->get_owning_dialog()->quit(0);
}

static void
pump(dialog* d)
{
	sMetaserverClient->pump();
}

static void
send_text(w_text_entry* te) {
	assert(te != NULL);

	// Make sure there's something worth sending
	if(strlen(te->get_text()) <= 0)
		return;

	dialog* d = te->get_owning_dialog();

	w_chat_history* ch = dynamic_cast<w_chat_history*>(d->get_widget_by_id(iCHAT_HISTORY));
	assert(ch != NULL);

	sMetaserverClient->sendChatMessage(std::string(te->get_text()));

	te->set_text("");
}

static void
setupAndConnectClient(MetaserverClient& client)
{
	{
		unsigned char* playerNameCStringStorage = pstrdup(player_preferences->name);
		char* playerNameCString = a1_p2cstr(playerNameCStringStorage);
		client.setPlayerName(playerNameCString);
		free(playerNameCStringStorage);
	}
	
	client.setPlayerTeamName("");
	client.connect("myth.mariusnet.com", 6321, "guest", "0000000000000000");
}

const IPaddress
run_network_metaserver_ui()
{
	obj_clear(sJoinAddress);
	MetaserverClient client;
	sMetaserverClient = &client;

	setupAndConnectClient(client);

	dialog d;

	NotificationAdapter	na(d);
	client.associateNotificationAdapter(&na);

	d.add(new w_static_text("LOCATE NETWORK GAMES", TITLE_FONT, TITLE_COLOR));

	d.add(new w_spacer());

	w_players_in_room* players_in_room_w = new w_players_in_room(client, &MetaserverClient::playersInRoom, NULL, 260, 8);
	players_in_room_w->set_identifier(iPLAYERS_IN_ROOM);
	players_in_room_w->set_alignment(widget::kAlignLeft);
	d.add(players_in_room_w);

	w_games_in_room* games_in_room_w = new w_games_in_room(client, &MetaserverClient::gamesInRoom, gameClicked, 320, 8);
	games_in_room_w->set_identifier(iGAMES_IN_ROOM);
	games_in_room_w->set_alignment(widget::kAlignRight);
	games_in_room_w->align_bottom_with_bottom_of(players_in_room_w);
	d.add(games_in_room_w);

	d.add(new w_spacer());

	w_chat_history* chat_history_w = new w_chat_history(600, 12);
	chat_history_w->set_identifier(iCHAT_HISTORY);
	d.add(chat_history_w);

	w_text_entry*	chatentry_w = new w_text_entry("Say:", 240, "");
	chatentry_w->set_identifier(iCHAT_ENTRY);
	chatentry_w->set_enter_pressed_callback(send_text);
	chatentry_w->set_alignment(widget::kAlignLeft);
	chatentry_w->set_full_width();
	d.add(chatentry_w);

	d.add(new w_spacer());

	d.add(new w_button("CANCEL", dialog_cancel, &d));

	d.set_processing_function(pump);

	int result = d.run();

	if(result == -1)
		obj_clear(sJoinAddress);

	sMetaserverClient = NULL;

	return sJoinAddress;
}


set<GameAvailableMetaserverAnnouncer*> GameAvailableMetaserverAnnouncer::s_instances;

GameAvailableMetaserverAnnouncer::GameAvailableMetaserverAnnouncer(const game_info& info)
{
	setupAndConnectClient(m_client);

	GameDescription description;
	description.m_type = info.net_game_type;
	description.m_timeLimit = info.time_limit;
	description.m_difficulty = info.difficulty_level;
	description.m_mapName = string(info.level_name);
	description.m_name = m_client.playerName() + "'s Game";
	description.m_teamsAllowed = !(info.game_options & _force_unique_teams);

	m_client.announceGame(GAME_PORT, description);

	s_instances.insert(this);
}

GameAvailableMetaserverAnnouncer::~GameAvailableMetaserverAnnouncer()
{
	s_instances.erase(this);
}

void
GameAvailableMetaserverAnnouncer::pump()
{
	m_client.pump();
}

void
GameAvailableMetaserverAnnouncer::pumpAll()
{
	for_each(s_instances.begin(), s_instances.end(), mem_fun(&GameAvailableMetaserverAnnouncer::pump));
}
