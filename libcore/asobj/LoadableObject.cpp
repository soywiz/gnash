// LoadableObject.cpp: abstraction of network-loadable AS object functions.
// 
//   Copyright (C) 2005, 2006, 2007, 2008 Free Software Foundation, Inc.
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
//

#include "LoadableObject.h"
#include "log.h"
#include "array.h"
#include "as_object.h"
#include "StreamProvider.h"
#include "URL.h"
#include "namedStrings.h"
#include "VM.h"
#include "builtin_function.h"
#include "timers.h"
#include "utf8.h"
#include "fn_call.h"

namespace gnash {


LoadableObject::LoadableObject()
    :
    _bytesLoaded(-1),
    _bytesTotal(-1),
    _loadCheckerTimer(0)
{
}    


LoadableObject::~LoadableObject()
{
    for (LoadThreadList::iterator it = _loadThreads.begin(),
            e = _loadThreads.end(); it != e; ++it)
    {
        // Joins the thread
        delete *it;
    }

    if ( _loadCheckerTimer )
    {
        _vm.getRoot().clear_interval_timer(_loadCheckerTimer);
    }

}

std::string
LoadableObject::getURLEncodedProperties()
{

	std::string qstring;

	typedef std::map<std::string, std::string> VarMap;
	VarMap vars;

	//return qstring;

	// TODO: it seems that calling enumerateProperties(vars) here
	//       somehow corrupts the stack !
	enumerateProperties(vars);

	for (VarMap::iterator it=vars.begin(), itEnd=vars.end();
			it != itEnd; ++it)
	{
		std::string var = it->first; URL::encode(var);
		std::string val = it->second; URL::encode(val);
		if ( it != vars.begin() ) qstring += "&";
		qstring += var + "=" + val;
	}

	return qstring;
}

void
LoadableObject::send(const std::string& /*urlstr*/)
{
    log_unimpl (__FUNCTION__);
}

void
LoadableObject::sendAndLoad(const std::string& urlstr,
                            as_object& target, bool post)
{

    /// All objects get a loaded member, set to false.
    target.set_member(NSV::PROP_LOADED, false);

	std::string querystring = getURLEncodedProperties();

	URL url(urlstr, get_base_url());

	std::auto_ptr<IOChannel> str;
	if (post)
    {
        as_value customHeaders;

        NetworkAdapter::RequestHeaders headers;

        if (get_member(NSV::PROP_uCUSTOM_HEADERS, &customHeaders))
        {

            /// Read in our custom headers if they exist and are an
            /// array.
            Array_as* array = dynamic_cast<Array_as*>(
                            customHeaders.to_object().get());
                            
            if (array)
            {
                Array_as::const_iterator e = array->end();
                --e;

                for (Array_as::const_iterator i = array->begin(); i != e; ++i)
                {
                    // Only even indices can be a header.
                    if (i.index() % 2) continue;
                    if (! (*i).is_string()) continue;
                    
                    // Only the immediately following odd number can be 
                    // a value.
                    if (array->at(i.index() + 1).is_string())
                    {
                        const std::string& name = (*i).to_string();
                        const std::string& val =
                                    array->at(i.index() + 1).to_string();
                                    
                        // Values should overwrite existing ones.
                        headers[name] = val;
                    }
                    
                }
            }
        }

        as_value contentType;

        if (get_member(NSV::PROP_CONTENT_TYPE, &contentType))
        {
            // This should not overwrite anything set in LoadVars.addRequestHeader();
            headers.insert(std::make_pair("Content-Type", contentType.to_string()));
        }

        /// It doesn't matter if there are no request headers.
        str = StreamProvider::getDefaultInstance().getStream(url,
                                                    querystring, headers);
    }
	else
    {
    	std::string url = urlstr + "?" + querystring;
        str = StreamProvider::getDefaultInstance().getStream(url);
    }

	if (!str.get()) 
	{
		log_error(_("Can't load from %s (security?)"), url.str());
		return;
		// TODO: check if this is correct
		//as_value nullValue; nullValue.set_null();
		//callMethod(VM::get().getStringTable().find(PROPNAME("onData")), nullValue);
	}

	log_security(_("Loading from url: '%s'"), url.str());
    target.queueLoad(str);
	
}

void
LoadableObject::load(const std::string& urlstr)
{
    // Set loaded property to false; will be updated (hopefully)
    // when loading is complete.
	set_member(NSV::PROP_LOADED, false);

	URL url(urlstr, get_base_url());

    // Checks whether access is allowed.
    std::auto_ptr<IOChannel> str(StreamProvider::getDefaultInstance().getStream(url));

	if (!str.get()) 
	{
		log_error(_("Can't load variables from %s (security?)"), url.str());
		return;
		// TODO: check if this is correct
		//as_value nullValue; nullValue.set_null();
		//callMethod(VM::get().getStringTable().find(PROPNAME("onData")), nullValue);
	}

	log_security(_("Loading from url: '%s'"), url.str());
    queueLoad(str);
}


void
LoadableObject::queueLoad(std::auto_ptr<IOChannel> str)
{

    bool startTimer = _loadThreads.empty();

    std::auto_ptr<LoadThread> lt ( new LoadThread(str) );

    // we push on the front to avoid invalidating
    // iterators when queueLoad is called as effect
    // of onData invocation.
    // Doing so also avoids processing queued load
    // request immediately
    // 
    _loadThreads.push_front(lt.get());
    lt.release();

    if ( startTimer )
    {
        boost::intrusive_ptr<builtin_function> loadsChecker = 
            new builtin_function(&checkLoads_wrapper);

        std::auto_ptr<Timer> timer(new Timer);
        timer->setInterval(*loadsChecker, 50, this);
        _loadCheckerTimer = getVM().getRoot().add_interval_timer(timer, true);

    }

    _bytesLoaded = 0;
    _bytesTotal = -1;

}

as_value
LoadableObject::checkLoads_wrapper(const fn_call& fn)
{
	boost::intrusive_ptr<LoadableObject> ptr = ensureType<LoadableObject>(fn.this_ptr);
	ptr->checkLoads();
	return as_value();
}


void
LoadableObject::checkLoads()
{

    if (_loadThreads.empty()) return; // nothing to do

    for (LoadThreadList::iterator it=_loadThreads.begin();
            it != _loadThreads.end(); )
    {
        LoadThread* lt = *it;

        if ( lt->completed() )
        {
            size_t xmlsize = _bytesTotal = _bytesLoaded = lt->getBytesTotal();
            boost::scoped_array<char> buf(new char[xmlsize+1]);
            size_t actuallyRead = lt->read(buf.get(), xmlsize);
            if ( actuallyRead != xmlsize )
			{
				// This would be either a bug of LoadThread or an expected
				// possibility which lacks documentation (thus a bug in documentation)
				//
			}
            buf[actuallyRead] = '\0';

            // Strip BOM, if any.
            // See http://savannah.gnu.org/bugs/?19915
            utf8::TextEncoding encoding;
            // NOTE: the call below will possibly change 'xmlsize' parameter
            char* bufptr = utf8::stripBOM(buf.get(), xmlsize, encoding);
            if ( encoding != utf8::encUTF8 && encoding != utf8::encUNSPECIFIED )
            {
                log_unimpl("%s to utf8 conversion in LoadVars input parsing", utf8::textEncodingName(encoding));
            }
            as_value dataVal(bufptr); // memory copy here (optimize?)

            it = _loadThreads.erase(it);
            delete lt; // supposedly joins the thread...

            // might push_front on the list..
            callMethod(NSV::PROP_ON_DATA, dataVal);

        }
        else
        {
            _bytesLoaded = lt->getBytesLoaded();
            ++it;
        }
    }

    if ( _loadThreads.empty() ) 
    {
        _vm.getRoot().clear_interval_timer(_loadCheckerTimer);
        _loadCheckerTimer=0;
    }
}



}
