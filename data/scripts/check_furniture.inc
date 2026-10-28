EventScript_PictureBookShelf::
	setvar VAR_0x8004, 1
	special Rogue_BufferRegionBook
	goto_if_eq VAR_RESULT, TRUE, EventScript_RogueLoreBook
	msgbox Text_PictureBookShelf, MSGBOX_SIGN
	end

@ Roguemon: the books of the region and of the family at home
@ (src/rogue_town.c).
EventScript_BookShelf::
	setvar VAR_0x8004, 0
	special Rogue_BufferRegionBook
	goto_if_eq VAR_RESULT, TRUE, EventScript_RogueLoreBook
	msgbox Text_BookShelf, MSGBOX_SIGN
	end

EventScript_PokemonCenterBookShelf::
	special Rogue_BufferLoreBook
	goto_if_eq VAR_RESULT, TRUE, EventScript_RogueLoreBook
	msgbox Text_PokemonCenterBookShelf, MSGBOX_SIGN
	end

@ Roguemon: a book of the region's legends (src/rogue_town.c).
EventScript_RogueLoreBook::
	msgbox gStringVar4, MSGBOX_SIGN
	end

EventScript_Vase::
	msgbox Text_Vase, MSGBOX_SIGN
	end

EventScript_EmptyTrashCan::
	msgbox Text_EmptyTrashCan, MSGBOX_SIGN
	end

EventScript_ShopShelf::
	msgbox Text_ShopShelf, MSGBOX_SIGN
	end

EventScript_Blueprint::
	msgbox Text_Blueprint, MSGBOX_SIGN
	end
