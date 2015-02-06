/**
 * @file math.hpp
 * @author Vaclav Blazek <vaclav.blazek@citationtech.net>
 *
 * 3D mesh operations
 */

#include "./meshop.hpp"
#include "./parse-obj.hpp"
#include "./triclip.hpp"

#include "utility/expect.hpp"
#include <boost/numeric/ublas/vector.hpp>

namespace geometry {

namespace ublas = boost::numeric::ublas;

Obj asObj(const Mesh &mesh)
{
    Obj obj;
    for ( math::Point3 vertex : mesh.vertices ) {

        geometry::Obj::Vector3d overtex;
        overtex.x = vertex[0]; overtex.y = vertex[1]; overtex.z = vertex[2];

        obj.addVertex( overtex );

        //LOG( info1 ) << "[" << overtex.x << ", " << overtex.y << ", "
        //    << overtex.z << "]";
    }

    for ( math::Point2 texture : mesh.tCoords ) {

        geometry::Obj::Vector3d otexture;
        otexture.x = texture[0]; otexture.y = texture[1]; otexture.z = 0.0;

        obj.addTexture( otexture );
    }

    for ( geometry::Face face : mesh.faces ) {

        geometry::Obj::Facet facet;

        facet.v[0] = face.a; facet.v[1] = face.b; facet.v[2] = face.c;
        facet.t[0] = face.ta; facet.t[1] = face.tb; facet.t[2] = face.tc;

        obj.addFacet( facet );
    }

    return obj;
}

Mesh::pointer asMesh(const Obj &obj){
    auto newMesh(std::make_shared<geometry::Mesh>());
    for( const auto&v : obj.vertices ){
        newMesh->vertices.push_back(v);
    }

    for( const auto&t : obj.texcoords ){
        newMesh->tCoords.emplace_back( t(0), t(1) );
    }

    for( const auto&f : obj.facets ){
        newMesh->addFace(f.v[0], f.v[1], f.v[2], f.t[0], f.t[1], f.t[2]);
    }

    return newMesh;
}

void saveAsObj(const Mesh &mesh, const boost::filesystem::path &filepath
               , const std::string &mtlName)
{
    LOG(info4) << "Saving mesh to file <" << filepath << ">.";

    std::ofstream out(filepath.string().c_str());
    out.setf(std::ios::scientific, std::ios::floatfield);

    out << "mtllib " << mtlName << '\n';

    for (const auto &vertex : mesh.vertices) {
        out << "v " << vertex(0) << ' ' << vertex(1) << ' '  << vertex(2)
            << '\n';
    }

    for (const auto &tCoord : mesh.tCoords) {
        out << "vt " << tCoord(0) << ' ' << tCoord(1) << '\n';
    }

    unsigned int currentImageId(static_cast<unsigned int>(-1));

    for (const auto &face : mesh.faces) {
        if (face.degenerate()) {
            continue;
        }
        if (face.imageId != currentImageId) {
            out << "usemtl " << face.imageId << '\n';
            currentImageId = face.imageId;
        }

        out << "f " << face.a + 1 << '/' << face.ta + 1 << "/ "
            << face.b + 1 << '/' << face.tb + 1 << "/ "
            << face.c + 1 << '/' << face.tc + 1 << "/\n";
    }

    if (!out) {
        LOGTHROW(err3, std::runtime_error)
            << "Unable to save mesh to <" << filepath << ">.";
    }
}

void saveAsPly( const Mesh &mesh, const boost::filesystem::path &filepath){
    LOG(info4) << "Saving mesh to file <" << filepath << ">.";

    std::ofstream out(filepath.string().c_str());
    out.setf(std::ios::scientific, std::ios::floatfield);

    unsigned validFaces(0);
    for (const auto &face : mesh.faces) {
        if (!face.degenerate() && mesh.good(face))
            validFaces++;
    }

    out << "ply\n"
        << "format ascii 1.0\n"
        << "comment generated by window-mesh\n"
        << "element vertex " << mesh.vertices.size() << '\n'
        << "property float x\n"
        << "property float y\n"
        << "property float z\n"
        << "element face " << validFaces << '\n'
        << "property list uchar int vertex_indices\n"
        << "end_header\n";

    for (const auto &vertex : mesh.vertices)
    {
        out << vertex(0) << ' ' << vertex(1) << ' '  << vertex(2) << '\n';
    }

    for (const auto &face : mesh.faces)
    {
        if (face.degenerate()) {
            continue;
        }
        if (!mesh.good(face)) {
            LOG(warn2) << "Invalid vertex index in face.";
            continue;
        }
        out << "3 " << face.a << ' ' << face.b << ' ' << face.c << '\n';
    }

    out.close();

    if (!out) {
        LOGTHROW(err3, std::runtime_error)
            << "Unable to save mesh to <" << filepath << ">.";
    }
}

Mesh loadPly( const boost::filesystem::path &filename )
{
    std::ifstream f(filename.native());
    if (!f.good()) {
        LOGTHROW(err2, std::runtime_error)
                << "Can't open " << filename;
    }

    f.exceptions(std::ios::badbit | std::ios::failbit);

    // read header
    std::string line;
    int nvert = -1, ntris = -1;
    do {
        if (getline(f, line).eof()) break;
        sscanf(line.c_str(), "element vertex %d", &nvert);
        sscanf(line.c_str(), "element face %d", &ntris);
    } while (line != "end_header");

    if (nvert < 0 || ntris < 0) {
        LOGTHROW(err2, std::runtime_error)
                << filename << ": unknown PLY format.";
    }

    Mesh mesh;

    // load points
    for (int i = 0; i < nvert; i++) {
        double x, y, z;
        f >> x >> y >> z;
        mesh.vertices.emplace_back(x, y, z);
    }

    // load triangles
    for (int i = 0; i < ntris; i++) {
        int n, a, b, c;
        f >> n >> a >> b >> c;
        utility::expect(n == 3, "Only triangles are supported in PLY files.");
        mesh.faces.emplace_back(a, b, c);
    }

    return mesh;
}

Mesh loadObj( const boost::filesystem::path &filename )
{
    struct Obj2MeshParser : public ObjParserBase {
        void addVertex( const Vector3d &v ) {
            mesh.vertices.push_back(v);
        }

        void addTexture( const Vector3d &t ) {
            mesh.tCoords.emplace_back(t.x, t.y);
        }

        void addFacet( const Facet &f ) {
            mesh.addFace( f.v[0], f.v[1], f.v[2]
                        , f.t[0], f.t[1], f.t[2] );
        }

        void addNormal( const Vector3d& ) { }
        void materialLibrary(const std::string&) { }
        void useMaterial(const std::string&) { }

        Mesh mesh;
    } parser_;

    std::ifstream file;
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file.open(filename.string());

    parser_.parse(file);

    return parser_.mesh;
}


Mesh::pointer clip( const Mesh& omesh, const math::Extents3& extents){
    auto pmesh(std::make_shared<geometry::Mesh>());

    ClipPlane planes[6];
    planes[0] = {+1.,  0., 0., extents.ll[0]};
    planes[1] = {-1.,  0., 0., -extents.ur[0]};
    planes[2] = {0.,  +1., 0., extents.ll[1]};
    planes[3] = {0.,  -1., 0., -extents.ur[1]};
    planes[4] = {0.,  0., +1., extents.ll[2]};
    planes[5] = {0.,  0., -1., -extents.ur[2]};
    ClipTriangle::list clipped;
    for (const auto& face : omesh.faces) {
        clipped.emplace_back(
              omesh.vertices[face.a]
            , omesh.vertices[face.b]
            , omesh.vertices[face.c]);
    }
    
    std::vector<double> tinfos;
    for (int i = 0; i < 6; i++) {
        clipped = clipTriangles(clipped, planes[i], tinfos);
    }
    
    std::map<math::Point3, math::Points3::size_type> pMap;
    math::Points3::size_type next=0;

    for (const auto &triangle : clipped) {
        math::Points3::size_type indices[3];
        for (int i = 0; i < 3; i++) {
            
            auto pair = pMap.insert(std::make_pair(triangle.pos[i], next));
            if (pair.second) next++;
            indices[i] = pair.first->second;
            
            if (indices[i] >= pmesh->vertices.size()) {
                pmesh->vertices.push_back( triangle.pos[i] );
            }
        }
        //do not add degenerated faces
        if ( (indices[0] != indices[1])
            && (indices[1] != indices[2])
            && (indices[0] != indices[2])) {  
            pmesh->addFace(indices[0], indices[1], indices[2]);
        }
    }

    return pmesh;
}


Mesh::pointer removeNonManifoldEdges( const Mesh& omesh ){
    auto pmesh(std::make_shared<geometry::Mesh>(omesh));
    auto & mesh(*pmesh);

    struct EdgeKey
    {
        int v1, v2; // vertex indices

        EdgeKey(int v1, int v2)
        {
            this->v1 = std::min(v1, v2);
            this->v2 = std::max(v1, v2);
        }

        bool operator< (const EdgeKey& other) const
        {
            return (v1 == other.v1) ? (v2 < other.v2) : (v1 < other.v1);
        }
    };

    struct Edge {
        std::set<size_t> facesIndices;
    };

    //count faces for each edge
    std::map<EdgeKey,Edge> edgeMap;
    for(uint fi=0; fi<mesh.faces.size(); fi++){
        const auto & face(mesh.faces[fi]);
        EdgeKey edgeKeys[3] = { EdgeKey(face.a,face.b)
                           , EdgeKey(face.b,face.c)
                           , EdgeKey(face.c,face.a) };
        for(const auto & key : edgeKeys){
            auto it = edgeMap.find(key);
            if(it==edgeMap.end()){
                //if edge is not present insert it with current face
                Edge edge;
                edge.facesIndices.insert(fi);
                edgeMap.insert(std::make_pair(key,edge));
            }
            else{
                //if edge is present add current face to it
                it->second.facesIndices.insert(fi);
            }
        }
    }


    //collect faces incident with non-manifold edge
    std::set<size_t> facesToOmit;
    for(auto it = edgeMap.begin(); it!=edgeMap.end(); it++){
        if(it->second.facesIndices.size()>2){
            for(const auto & fi : it->second.facesIndices){
                facesToOmit.insert(fi);
            }
        }
    }

    mesh.faces.clear();
    for(uint fi=0; fi<omesh.faces.size(); fi++){
        const auto & face(omesh.faces[fi]);
        if(facesToOmit.find(fi)==facesToOmit.end()){
            mesh.addFace( face.a, face.b, face.c
                        , face.ta, face.tb, face.tc );
        }
    }

    return pmesh;
}


Mesh::pointer refine( const Mesh & omesh, uint maxFacesCount){
    auto pmesh(std::make_shared<geometry::Mesh>(omesh));
    auto & mesh(*pmesh);

    struct EdgeKey
    {
        int v1, v2; // vertex indices

        EdgeKey(int v1, int v2)
        {
            this->v1 = std::min(v1, v2);
            this->v2 = std::max(v1, v2);
        }

        bool operator< (const EdgeKey& other) const
        {
            return (v1 == other.v1) ? (v2 < other.v2) : (v1 < other.v1);
        }
    };

    struct Edge {
        typedef enum {
            AB,
            BC,
            CA
        } EdgeType;

        int v1, v2;
        int f1, f2;
        EdgeType et1, et2;

        float length;

        Edge(int pv1, int pv2, float plength)
            :v1(std::min(pv1,pv2)),v2(std::max(pv1,pv2)), length(plength){
            f1 = -1;
            f2 = -1;
        }

        void addFace(int pv1, int pv2, int fid, EdgeType type){
            if(pv1<pv2){
                f1=fid;
                et1 = type;
            }
            else{
                f2=fid;
                et2 = type;
            }
        }

        bool operator< (const Edge& other) const
        {
            return length < other.length;
        }
    };

    struct EdgeMap {
        std::map<EdgeKey, std::shared_ptr<Edge>> map;
        std::vector<std::shared_ptr<Edge>> heap;

        bool compareEdgePtr(const std::shared_ptr<Edge> &a, const std::shared_ptr<Edge> &b){
            return a->length<b->length;
        }

        void addFaceEdge(int pv1, int pv2, int fid, Edge::EdgeType type, float length){
            auto key(EdgeKey(pv1,pv2));
            auto it(map.find(key));
            if(it!=map.end()){
                it->second->addFace(pv1, pv2, fid, type );
            }
            else{
                heap.push_back(std::make_shared<Edge>(Edge(pv1, pv2, length)));
                heap.back()->addFace(pv1, pv2, fid, type );
                map.insert(std::make_pair(key,heap.back()));
                std::push_heap(heap.begin(),heap.end(), [this]( const std::shared_ptr<Edge> &a
                                                              , const std::shared_ptr<Edge> &b){
                    return this->compareEdgePtr(a,b);
                });
            }
        }

        Edge pop_top_edge(){
            auto edge = *heap[0];
            std::pop_heap(heap.begin(), heap.end(), [this]( const std::shared_ptr<Edge> &a
                                                          , const std::shared_ptr<Edge> &b){
                return this->compareEdgePtr(a,b);
            });
            heap.pop_back();
            map.erase(EdgeKey(edge.v1, edge.v2));
            return edge;
        }

        Edge top_edge(){
            return *heap[0];
        }

        void addFaceEdges(const Mesh & mesh, uint fid){
            const auto& f = mesh.faces[fid];
            float e1Length =  ublas::norm_2(mesh.vertices[f.a]-mesh.vertices[f.b]);
            addFaceEdge(f.a, f.b, fid, Edge::EdgeType::AB, e1Length);

            float e2Length =  ublas::norm_2(mesh.vertices[f.b]-mesh.vertices[f.c]);
            addFaceEdge(f.b, f.c, fid, Edge::EdgeType::BC, e2Length);

            float e3Length =  ublas::norm_2(mesh.vertices[f.c]-mesh.vertices[f.a]);
            addFaceEdge(f.c, f.a, fid, Edge::EdgeType::CA, e3Length);
        }

        std::size_t size(){
            return heap.size();
        }

    };

    EdgeMap edgeMap;

    auto splitEdge = [&mesh,&edgeMap]( int fid, Edge::EdgeType type
                       , int vid) mutable -> void{
        auto & face = mesh.faces[fid];
        switch(type){
            case Edge::EdgeType::AB:
                {
                    if(mesh.tCoords.size()>0){
                        math::Point2 tcMiddle = ( mesh.tCoords[face.ta]
                                                + mesh.tCoords[face.tb]) * 0.5;
                        mesh.tCoords.push_back(tcMiddle);
                    }
                    mesh.addFace( mesh.faces[fid].b, mesh.faces[fid].c
                                , vid
                                , mesh.faces[fid].tb, mesh.faces[fid].tc
                                , mesh.tCoords.size()-1);

                    mesh.faces[fid].b = vid;
                    mesh.faces[fid].tb = mesh.tCoords.size()-1;

                    edgeMap.addFaceEdges(mesh, fid);
                    edgeMap.addFaceEdges(mesh, mesh.faces.size()-1);
                }
                break;
            case Edge::EdgeType::BC:
                {
                    if(mesh.tCoords.size()>0){
                        math::Point2 tcMiddle = (mesh.tCoords[face.tb]
                                                + mesh.tCoords[face.tc]) * 0.5;
                        mesh.tCoords.push_back(tcMiddle);
                    }
                    mesh.addFace( mesh.faces[fid].c,mesh.faces[fid].a, vid
                                , mesh.faces[fid].tc,mesh.faces[fid].ta
                                , mesh.tCoords.size()-1);

                    mesh.faces[fid].c = vid;
                    mesh.faces[fid].tc = mesh.tCoords.size()-1;

                    edgeMap.addFaceEdges(mesh, fid);
                    edgeMap.addFaceEdges(mesh, mesh.faces.size()-1);
                }
                break;
            case Edge::EdgeType::CA:
                {
                    if(mesh.tCoords.size()>0){
                        math::Point2 tcMiddle = (mesh.tCoords[face.tc]
                                                + mesh.tCoords[face.ta]) * 0.5;
                        mesh.tCoords.push_back(tcMiddle);
                    }

                    mesh.addFace( mesh.faces[fid].a,mesh.faces[fid].b, vid
                                , mesh.faces[fid].ta,mesh.faces[fid].tb
                                , mesh.tCoords.size()-1);

                    mesh.faces[fid].a = vid;
                    mesh.faces[fid].ta = mesh.tCoords.size()-1;

                    edgeMap.addFaceEdges(mesh, fid);
                    edgeMap.addFaceEdges(mesh, mesh.faces.size()-1);
                }
                break;
        }
    };

    for (uint i=0; i<mesh.faces.size(); ++i ) {
        //add all 3 edges
        edgeMap.addFaceEdges(mesh, i);
    }

    //sort edges by length
    while( mesh.faces.size() < maxFacesCount && edgeMap.size()>0 ){
        //split edge
        auto edge = edgeMap.pop_top_edge();

        //find middle
        math::Point3 middle = (mesh.vertices[edge.v1]
                            + mesh.vertices[edge.v2]) * 0.5;
        mesh.vertices.push_back(middle);

        //split first face
        if(edge.f1>=0){
            splitEdge(edge.f1, edge.et1, mesh.vertices.size()-1);
        }

        //split second face
        if(edge.f2>=0){
            splitEdge(edge.f2, edge.et2, mesh.vertices.size()-1);
        }
    }

    return pmesh;
}


} // namespace geometry
