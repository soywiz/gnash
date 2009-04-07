// shape.h	-- Thatcher Ulrich <tu@tulrich.com> 2003

// This source code has been donated to the Public Domain.  Do
// whatever you want with it.

// Quadratic bezier outline shapes, the basis for most SWF rendering.


#ifndef GNASH_SHAPE_CHARACTER_DEF_H
#define GNASH_SHAPE_CHARACTER_DEF_H

#include "character_def.h" // for inheritance of shape_character_def
#include "smart_ptr.h" // GNASH_USE_GC
#include "swf.h"
#include "ShapeRecord.h"

namespace gnash {
	class SWFStream;
	class cxform;
    class Shape;
	class SWFMatrix;
}

namespace gnash {

/// \brief
/// Represents the outline of one or more shapes, along with
/// information on fill and line styles.
//
class shape_character_def : public character_def
{
public:

    shape_character_def(SWFStream& in, SWF::TagType tag, movie_definition& m);

    virtual ~shape_character_def() {};

    // Display a Shape character.
    virtual void display(const DisplayObject& inst) const;

    // Create a Shape DisplayObject.
	virtual DisplayObject* createDisplayObject(DisplayObject* parent, int id);
	
    /// Get cached bounds of this shape.
    const rect&	get_bound() const { return _shape.getBounds(); }

    virtual bool pointTestLocal(boost::int32_t x, boost::int32_t y, 
            const SWFMatrix& wm) const;

protected:

#ifdef GNASH_USE_GC
    /// Mark reachable resources (for the GC)
    //
    /// Reachable resources are:
    ///	- Associated fill styles (_fill_styles).
    ///	  These are not actual resources, but may contain some.
    ///
    virtual void markReachableResources() const;
#endif 

private:

    const SWF::ShapeRecord _shape;

};

} // namespace gnash


#endif 


// Local Variables:
// mode: C++
// c-basic-offset: 8 
// tab-width: 8
// indent-tabs-mode: t
// End:
