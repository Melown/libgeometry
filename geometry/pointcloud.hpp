/**
 * @file pointcloud.hpp
 * @author Ondrej Prochazka <ondrej.prochazka@citationtech.net>
 *
 * Point clouds, or set of 3D points (usually surface boundary samples).
 */

#ifndef GEOMETRY_POINTCLOUD_HPP
#define GEOMETRY_POINTCLOUD_HPP

#include <set>

#include <boost/numeric/ublas/vector.hpp>
#include <boost/numeric/ublas/io.hpp>

#include "math/geometry.hpp"

namespace ublas = boost::numeric::ublas;

namespace geometry {

/**
 * PointCloud is essentially a std::vector of 3D points, with extents
 * maintenance and sampling density computation.
 */

class PointCloud: public std::vector<math::Point3> {

public :

    /** Initialize */
    PointCloud() :
        _lower( ublas::zero_vector<double>( 3 ) ),
        _upper( ublas::zero_vector<double>( 3 ) ) {};

    /** push_back */
    void push_back ( const math::Point3 & x );

    /** insert */
    iterator insert ( iterator position, const math::Point3 & x );

    /** insert */
    void insert ( iterator position, size_type n,
                  const math::Point3 & x );

    /** insert */
    template <class InputIterator>
    void insert ( iterator position, InputIterator first, InputIterator last );

    /** clear */
    void clear();

    /**
     * Save to a file. The format is simplistic, with one line per point,
     * three whitespace separated values per line.
     */
    void dump( const std::string & path );

    /**
     * Load a file saved with "dump".
     */
    void load( const std::string & path );

    /**
     * Return a measure of euclidian distance to a nearest point.
     * Sampling delta * 100 % point are at most as far from a nearest point as
     * the return value.
     */
    double samplingDelta ( float bulkThreshold = 0.5 ) const;

    /** Upper bound of all points */
    math::Point3 upper() const { assert( ! empty() ); return _upper; }

    /** Upper bound of all points. */
    math::Point3 lower() const { assert( ! empty() ); return _lower; }

private :

    /* forbidden modifiers */
    template <class InputIterator>
    void assign ( InputIterator first, InputIterator last ) {
        assert( false ); }
    void pop_back ( ) { assert( false); }
    iterator erase ( iterator position ) {
        (void) position; assert( false ); return position; }
    iterator erase ( iterator first, iterator last ) {
        (void) first; (void) last;
        assert( false ); return first;
    }
    void swap( std::vector<ublas::vector< double> >& vec ) {
        (void) vec;
        assert( false ); }

    void updateExtents( const math::Point3 & x );

    class ThreeDistance {

    public :

        ThreeDistance( double value = 0.0 )
            : distX( value ), distY( value ), distZ( value ) {}

        void update( const math::Point3 diff );

        double value() const;

        bool operator < ( const ThreeDistance & op ) const {

            return value() < op.value();
        }
    private :

        double distX, distY, distZ;
    };

    math::Point3 _lower, _upper;
};


/* template method implementation */

template <class InputIterator>
void PointCloud::insert ( iterator position, InputIterator first,
                            InputIterator last ) {

    for ( InputIterator it = first; it < last; it++ )
        updateExtents( *it );

    std::vector<math::Point3>::insert( position, first, last );
}


} // namespace geometry

#endif
