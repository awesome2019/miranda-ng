#pragma once

#include "common.h"

class CContact : public Contact
{
public:
	typedef DRef<CContact, Contact> Ref;
	typedef DRefs<CContact, Contact> Refs;

	CContact(CSkypeProto*, unsigned int oid, SERootObject* root);

	SEString GetSid();
	SEString GetNick();
	bool GetFullname(SEString &firstName, SEString &lastName);

private:
	CSkypeProto* proto;

	void OnChange(int prop);
};