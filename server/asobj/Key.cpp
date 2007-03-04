// 
//   Copyright (C) 2005, 2006, 2007 Free Software Foundation, Inc.
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.

// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//
//

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "log.h"
#include "Key.h"
#include "fn_call.h"
#include "movie_root.h"
#include "action.h" // for call_method
#include "VM.h"
#include "builtin_function.h"

#include <boost/algorithm/string/case_conv.hpp>

namespace gnash {

Key::Key() {
}

Key::~Key() {
}


void
Key::addListener()
{
    log_msg("%s:unimplemented \n", __FUNCTION__);
}

void
Key::getAscii()
{
    log_msg("%s:unimplemented \n", __FUNCTION__);
}

void
Key::getCode()
{
    log_msg("%s:unimplemented \n", __FUNCTION__);
}

void
Key::isDown()
{
    log_msg("%s:unimplemented \n", __FUNCTION__);
}

void
Key::isToggled()
{
    log_msg("%s:unimplemented \n", __FUNCTION__);
}

void
Key::removeListener()
{
    log_msg("%s:unimplemented \n", __FUNCTION__);
}
void
key_new(const fn_call& fn)
{
    key_as_object *key_obj = new key_as_object;
    fn.result->set_as_object(key_obj);
}

/************************************************************************
 *
 * This has been moved from action.cpp, when things are clean
 * everything should have been moved up
 *
 ************************************************************************/

key_as_object::key_as_object()
	:
	m_last_key_pressed(0)
{
    memset(m_keymap, 0, sizeof(m_keymap));
}

bool
key_as_object::is_key_down(int code)
{
	    if (code < 0 || code >= key::KEYCOUNT) return false;

	    int	byte_index = code >> 3;
	    int	bit_index = code - (byte_index << 3);
	    int	mask = 1 << bit_index;

	    assert(byte_index >= 0 && byte_index < int(sizeof(m_keymap)/sizeof(m_keymap[0])));

	    if (m_keymap[byte_index] & mask)
		{
		    return true;
		}
	    else
		{
		    return false;
		}
}

void
key_as_object::set_key_down(int code)
{
	    if (code < 0 || code >= key::KEYCOUNT) return;

	    m_last_key_pressed = code;

	    int	byte_index = code >> 3;
	    int	bit_index = code - (byte_index << 3);
	    int	mask = 1 << bit_index;

	    assert(byte_index >= 0 && byte_index < int(sizeof(m_keymap)/sizeof(m_keymap[0])));

	    m_keymap[byte_index] |= mask;

	std::string funcname = event_id(event_id::KEY_DOWN).get_function_name();
	VM& vm = VM::get();
	if ( vm.getSWFVersion() < 7 )
	{
		boost::to_lower(funcname, vm.getLocale());
	}
	notify_listeners(funcname);
}


void
key_as_object::set_key_up(int code)
{
	    if (code < 0 || code >= key::KEYCOUNT) return;

	    int	byte_index = code >> 3;
	    int	bit_index = code - (byte_index << 3);
	    int	mask = 1 << bit_index;

	    assert(byte_index >= 0 && byte_index < int(sizeof(m_keymap)/sizeof(m_keymap[0])));

	    m_keymap[byte_index] &= ~mask;

	std::string funcname = event_id(event_id::KEY_UP).get_function_name();
	VM& vm = VM::get();
	if ( vm.getSWFVersion() < 7 )
	{
		boost::to_lower(funcname, vm.getLocale());
	}
	notify_listeners(funcname);
}

// TODO: take a std::string
void
key_as_object::notify_listeners(const std::string& funcname)
{
    // Notify listeners.
    for (std::vector<boost::intrusive_ptr<as_object> >::iterator iter = m_listeners.begin();
         iter != m_listeners.end(); ++iter) {
      if (*iter == NULL)
        continue;

      boost::intrusive_ptr<as_object>  listener = *iter; // Hold an owning reference.
      as_value method;

      if (listener->get_member(funcname.c_str(), &method))
        call_method(method, NULL /* or root? */, listener.get(), 0, 0);
    }
}

void
key_as_object::add_listener(as_object* listener)
{

    // Should we bother doing this every time someone calls add_listener(),
    // or should we perhaps skip this check and use unique later?
    std::vector<boost::intrusive_ptr<as_object> >::const_iterator end = m_listeners.end();
    for (std::vector<boost::intrusive_ptr<as_object> >::iterator iter = m_listeners.begin();
         iter != end; ++iter) {
      if (*iter == NULL) {
        // Already in the list.
        return;
      }
    }

    m_listeners.push_back(listener);
}

void
key_as_object::remove_listener(as_object* listener)
{

  for (std::vector<boost::intrusive_ptr<as_object> >::iterator iter = m_listeners.begin(); iter != m_listeners.end(); )
	{
    if (*iter == listener)
		{
      iter = m_listeners.erase(iter);
			continue;
    }
		iter++;
	}
}

int
key_as_object::get_last_key_pressed() const
{
	return m_last_key_pressed;
}


void
key_add_listener(const fn_call& fn)
{
    if (fn.nargs < 1)
	{
	    log_error("key_add_listener needs one argument (the listener object)\n");
	    return;
	}

    as_object*	listener = fn.arg(0).to_object();
    if (listener == NULL)
	{
	    log_error("key_add_listener passed a NULL object; ignored\n");
	    return;
	}

    key_as_object*	ko = static_cast<key_as_object*>( fn.this_ptr );
    assert(ko);

    ko->add_listener(listener);
}

void	key_get_ascii(const fn_call& fn)
    // Return the ascii value of the last key pressed.
{
    key_as_object*	ko = static_cast<key_as_object*>( fn.this_ptr );
    assert(ko);

    fn.result->set_undefined();

    int	code = ko->get_last_key_pressed();
    if (code > 0)
	{
	    // @@ Crude for now; just jamming the key code in a string, as a character.
	    // Need to apply shift/capslock/numlock, etc...
	    char	buf[2];
	    buf[0] = (char) code;
	    buf[1] = 0;

	    fn.result->set_string(buf);
	}
}

void	key_get_code(const fn_call& fn)
    // Returns the keycode of the last key pressed.
{
    key_as_object*	ko = static_cast<key_as_object*>( fn.this_ptr );
    assert(ko);

    fn.result->set_int(ko->get_last_key_pressed());
}

void	key_is_down(const fn_call& fn)
    // Return true if the specified (first arg keycode) key is pressed.
{
    if (fn.nargs < 1)
	{
	    log_error("key_is_down needs one argument (the key code)\n");
	    return;
	}

    int	code = (int) fn.arg(0).to_number();

    key_as_object*	ko = static_cast<key_as_object*>( fn.this_ptr );
    assert(ko);

    fn.result->set_bool(ko->is_key_down(code));
}

void	key_is_toggled(const fn_call& fn)
    // Given the keycode of NUM_LOCK or CAPSLOCK, returns true if
    // the associated state is on.
{
    // @@ TODO
    fn.result->set_bool(false);
}

void	key_remove_listener(const fn_call& fn)
    // Remove a previously-added listener.
{
    if (fn.nargs < 1)
	{
	    log_error("key_remove_listener needs one argument (the listener object)\n");
	    return;
	}

    as_object*	listener = fn.arg(0).to_object();
    if (listener == NULL)
	{
	    log_error("key_remove_listener passed a NULL object; ignored\n");
	    return;
	}

    key_as_object*	ko = static_cast<key_as_object*>( fn.this_ptr );
    assert(ko);

    ko->remove_listener(listener);
}

void	notify_key_event(key::code k, bool down)
    // External interface for the host to report key events.
{
//	    GNASH_REPORT_FUNCTION;
	    
	// Notify keypress listeners.
	if (down) 
	{
		movie_root& mroot = VM::get().getRoot();
		mroot.notify_keypress_listeners(k);
	}

	//
	// Notify the _global.Key object about key event
	//


	VM& vm = VM::get();
	if ( vm.getSWFVersion() < 6 )
	{
		// _global.Key was added in SWF6
		return;
	}

	static boost::intrusive_ptr<key_as_object> keyobject = NULL;
	if ( ! keyobject )
	{
		// This isn't very performant... do we allow user override
		// of _global.Key, btw ?

		as_value kval;
		as_object* global = VM::get().getGlobal();

		std::string objName = "Key";
		if ( vm.getSWFVersion() < 7 )
		{
			boost::to_lower(objName, vm.getLocale());
		}
		if ( global->get_member(objName, &kval) )
		{
			//log_msg("Found member 'Key' in _global: %s", kval.to_string());
			as_object* obj = kval.to_object();
			//log_msg("_global.Key to_object() : %s @ %p", typeid(*obj).name(), obj);
			keyobject = dynamic_cast<key_as_object*>( obj );
		}
	}

	if ( keyobject )
	{
		if (down) keyobject->set_key_down(k);
		else keyobject->set_key_up(k);
	}
	else
	{
		log_error("gnash::notify_key_event(): _global.Key doesn't exist, or isn't the expected built-in\n");
	}
}

void key_class_init(as_object& global)
{

//  GNASH_REPORT_FUNCTION;

    // Create built-in key object.
    // NOTE: _global.Key *is* an object, not a constructor
    as_object*	key_obj = new key_as_object;

    // constants
#define KEY_CONST(k) key_obj->set_member(#k, key::k)
    KEY_CONST(BACKSPACE);
    KEY_CONST(CAPSLOCK);
    KEY_CONST(CONTROL);
    KEY_CONST(DELETEKEY);
    KEY_CONST(DOWN);
    KEY_CONST(END);
    KEY_CONST(ENTER);
    KEY_CONST(ESCAPE);
    KEY_CONST(HOME);
    KEY_CONST(INSERT);
    KEY_CONST(LEFT);
    KEY_CONST(PGDN);
    KEY_CONST(PGUP);
    KEY_CONST(RIGHT);
    KEY_CONST(SHIFT);
    KEY_CONST(SPACE);
    KEY_CONST(TAB);
    KEY_CONST(UP);

    // methods
	key_obj->init_member("addListener", new builtin_function(key_add_listener));
	key_obj->init_member("getAscii", new builtin_function(key_get_ascii));
	key_obj->init_member("getCode", new builtin_function(key_get_code));
	key_obj->init_member("isDown", new builtin_function(key_is_down));
	key_obj->init_member("isToggled", new builtin_function(key_is_toggled));
	key_obj->init_member("removeListener", new builtin_function(key_remove_listener));


    global.init_member("Key", key_obj);
}

} // end of gnash namespace

