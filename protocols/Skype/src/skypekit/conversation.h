#pragma once

#include "common.h"

class CConversation : public Conversation
{
public:
	typedef void (CSkypeProto::* OnConvoChanged)(const ConversationRef &conversation, int);

	typedef DRef<CConversation, Conversation> Ref;
	typedef DRefs<CConversation, Conversation> Refs;

	CConversation(unsigned int oid, SERootObject* root);
};