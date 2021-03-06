/*

Jabber Protocol Plugin for Miranda NG

Copyright (c) 2002-04  Santithorn Bunchua
Copyright (c) 2005-12  George Hazan
Copyright (c) 2007     Artem Shpynov
Copyright (C) 2012-19 Miranda NG team

Module implements a search according to XEP-0055: Jabber Search
http://www.xmpp.org/extensions/xep-0055.html

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#pragma once

struct JabberSearchFieldsInfo
{
	wchar_t * szFieldName;
	wchar_t * szFieldCaption;
	HWND hwndCaptionItem;
	HWND hwndValueItem;
};

struct JabberSearchData : public MZeroedObject
{
	struct CJabberProto *ppro;
	JabberSearchFieldsInfo *pJSInf;
	TiXmlDocument doc;
	TiXmlElement *xNode;
	int nJSInfCount;
	int lastRequestIq;
	int CurrentHeight;
	int curPos;
	int frameHeight;
	RECT frameRect;
	BOOL fSearchRequestIsXForm;
};

struct Data
{
	wchar_t *Label;
	wchar_t * Var;
	wchar_t * defValue;
	BOOL  bHidden;
	BOOL  bReadOnly;
	int Order;

};

static HWND searchHandleDlg = nullptr;

// Implementation of MAP class (the list
template <typename _KEYTYPE, int(*COMPARATOR)(_KEYTYPE*, _KEYTYPE*) >
class UNIQUE_MAP
{

public:
	typedef _KEYTYPE* (*COPYKEYPROC)(_KEYTYPE*);
	typedef void(*DESTROYKEYPROC)(_KEYTYPE*);

private:
	typedef struct _tagRECORD
	{
		_tagRECORD(_KEYTYPE * key, wchar_t * value = nullptr) { _key = key; _value = value; _order = 0; _destroyKeyProc = nullptr; }
		~_tagRECORD()
		{
			if (_key && _destroyKeyProc)
				_destroyKeyProc(_key);
			_key = nullptr;
			_destroyKeyProc = nullptr;
		}
		_KEYTYPE *_key;
		wchar_t * _value;
		int _order;
		DESTROYKEYPROC _destroyKeyProc;
	} _RECORD;

	int _nextOrder;
	LIST<_RECORD> _Records;

	static int _KeysEqual(const _RECORD *p1, const _RECORD *p2)
	{
		if (COMPARATOR)
			return (int)(COMPARATOR((p1->_key), (p2->_key)));
		else
			return (int)(p1->_key < p2->_key);
	}

	inline int _remove(_RECORD* p)
	{
		int _itemOrder = p->_order;
		if (_Records.remove(p)) {
			delete(p);
			_nextOrder--;
			for (auto &temp : _Records)
				if (temp && temp->_order > _itemOrder)
					temp->_order--;
			return 1;
		}
		return 0;
	}
	inline _RECORD * _getUnorderedRec(int index)
	{
		for (auto &rec : _Records)
			if (rec->_order == index)
				return rec;

		return nullptr;
	}

public:
	UNIQUE_MAP(int incr) :_Records(incr, _KeysEqual)
	{
		_nextOrder = 0;
	};
	~UNIQUE_MAP()
	{
		_RECORD * record;
		int i = 0;
		while (record = _Records[i++]) delete record;
	}

	int insert(_KEYTYPE* Key, wchar_t *Value)
	{
		_RECORD * rec = new _RECORD(Key, Value);
		int index = _Records.getIndex(rec);
		if (index < 0) {
			if (!_Records.insert(rec)) delete rec;
			else {
				index = _Records.getIndex(rec);
				rec->_order = _nextOrder++;
			}
		}
		else {
			_Records[index]->_value = Value;
			delete rec;
		}
		return index;
	}
	int insertCopyKey(_KEYTYPE* Key, wchar_t *Value, _KEYTYPE** _KeyReturn, COPYKEYPROC CopyProc, DESTROYKEYPROC DestroyProc)
	{
		_RECORD * rec = new _RECORD(Key, Value);
		int index = _Records.getIndex(rec);
		if (index < 0) {
			_KEYTYPE* newKey = CopyProc(Key);
			if (!_Records.insert(rec)) {
				delete rec;
				DestroyProc(newKey);
				if (_KeyReturn) *_KeyReturn = nullptr;
			}
			else {
				rec->_key = newKey;
				rec->_destroyKeyProc = DestroyProc;
				index = _Records.getIndex(rec);
				rec->_order = _nextOrder++;
				if (_KeyReturn) *_KeyReturn = newKey;
			}
		}
		else {
			_Records[index]->_value = Value;
			if (_KeyReturn) *_KeyReturn = _Records[index]->_key;
			delete rec;
		}
		return index;
	}
	inline wchar_t* operator[](_KEYTYPE* _KEY) const
	{
		_RECORD rec(_KEY);
		int index = _Records.getIndex(&rec);
		_RECORD * rv = _Records[index];
		if (rv) {
			if (rv->_value)
				return rv->_value;
			else
				return L"";
		}
		else
			return nullptr;
	}
	inline wchar_t* operator[](int index) const
	{
		_RECORD * rv = _Records[index];
		if (rv) return rv->_value;
		else return nullptr;
	}
	inline _KEYTYPE* getKeyName(int index)
	{
		_RECORD * rv = _Records[index];
		if (rv) return rv->_key;
		else return nullptr;
	}
	inline wchar_t * getUnOrdered(int index)
	{
		_RECORD * rec = _getUnorderedRec(index);
		if (rec) return rec->_value;
		else return nullptr;
	}
	inline _KEYTYPE * getUnOrderedKeyName(int index)
	{
		_RECORD * rec = _getUnorderedRec(index);
		if (rec) return rec->_key;
		else return nullptr;
	}
	inline int getCount()
	{
		return _Records.getCount();
	}
	inline int removeUnOrdered(int index)
	{
		_RECORD * p = _getUnorderedRec(index);
		if (p) return _remove(p);
		else return 0;
	}
	inline int remove(int index)
	{
		_RECORD * p = _Records[index];
		if (p) return _remove(p);
		else return 0;
	}
	inline int getIndex(_KEYTYPE * key)
	{
		_RECORD temp(key);
		return _Records.getIndex(&temp);
	}
};

inline int TCharKeyCmp(wchar_t* a, wchar_t* b)
{
	return (int)(mir_wstrcmpi(a, b));
}

inline int CharKeyCmp(char *a, char *b)
{
	return mir_strcmpi(a, b);
}
