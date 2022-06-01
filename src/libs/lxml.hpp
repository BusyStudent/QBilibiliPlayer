#pragma once

#define LXML_CXX17 (__cplusplus >= 201703L)

#ifndef LXML_U8STRING_VIEW
    #if !LXML_CXX17
        #define LXML_U8STRING_VIEW const LXML_U8STRING &
    #else
        #define LXML_U8STRING_VIEW std::string_view
        #include <string_view>
    #endif
#endif

#if LXML_CXX17
    #define LXML_CONSTEXPR constexpr
#else
    #define LXML_CONSTEXPR const
#endif

#ifndef LXML_U8STRING
    #define LXML_U8STRING std::string
    #include <string>
#endif

#ifndef LXML_CHAR
    #define LXML_CHAR LXML_U8STRING::value_type
#endif

#ifndef LXML_NAMESPACE
    #define LXML_NAMESPACE LXml
#endif

#ifdef LXML_STATIC
    #define LXML_NS_BEING \
        namespace {\
        namespace LXML_NAMESPACE {
    #define LXML_NS_END \
        }\
        }
#else
    #define LXML_NS_BEGIN \
        namespace LXML_NAMESPACE {
    #define LXML_NS_END \
        }
#endif

#ifndef LXML_ASSERT
    #define LXML_ASSERT(X) assert(X)
    #include <cassert>
#endif

#ifndef LXML_NO_EXCEPTIONS
    #include <stdexcept>
    #define LXML_CHECK(COND) if(!(COND)) throw std::runtime_error("LXML_CHECK failed: " #COND)
    #define LXML_THROW(X) throw X
#else
    #define LXML_CHECK(COND) LXML_ASSERT(COND)
    #define LXML_THROW(X) LXML_ASSERT(false)
#endif

//--Import libxml2 headers
#include <libxml/HTMLparser.h>
#include <libxml/HTMLtree.h>
#include <libxml/xmlversion.h>
#include <libxml/xpath.h>
#include <libxml/tree.h>
//--Import std headers
#include <type_traits>

//--Detail of progress
//--TODO : Add a special class to manage mem return by libxml2,avoid useless copy
//         Such as LXml::lstring or LXml::string or LXml::ustring
//--TODO : Add SAXParser
//--TODO : Add PushParser



LXML_NS_BEGIN

using u8string_view = LXML_U8STRING_VIEW;
using u8string = LXML_U8STRING;
using char_t = LXML_CHAR;
//--Common class reference
class DocumentRef;
class NodeRef;
//--Common class
class XPathExpression;
class XPathNodeSet;
class XPathContent;
class XPathObject;
class Document;
class Node;
//--Common constants
enum ParseOptions : int {
    NoBlanks = XML_PARSE_NOBLANKS,
    NoError = XML_PARSE_NOERROR,
    NoWarning = XML_PARSE_NOWARNING,
    NoNetwork = XML_PARSE_NONET,
    NoXinclude = XML_PARSE_NOXINCNODE,
    NoEntities = XML_PARSE_NOENT,
    Recover = XML_PARSE_RECOVER,
};
LXML_CONSTEXPR int DefaultOptions = NoBlanks | NoError | NoWarning | NoNetwork | Recover;

//--LXml String
inline u8string ToString(xmlChar *s) {
    if(s == nullptr){
        return u8string();
    }
    u8string us(reinterpret_cast<char_t *>(s));
    xmlFree(s);
    return us;
}
inline u8string ToString(const xmlChar *s) {
    if(s == nullptr){
        return u8string();
    }
    u8string us(reinterpret_cast<const char_t *>(s));
    return us;
}
/**
 * @brief Reference to a document
 * 
 */
class DocumentRef {
    public:
        //--Constructors
        DocumentRef() = default;
        DocumentRef(xmlDocPtr p) : doc(p) {};
        DocumentRef(const DocumentRef &) = default;
        DocumentRef(DocumentRef &&) = default;
        ~DocumentRef() = default;
        //--Operators
        DocumentRef &operator =(const DocumentRef &) = default;
        DocumentRef &operator =(DocumentRef &&) = default;

    public:
        /**
         * @brief Get Root Node
         * 
         * @return NodeRef 
         */
        NodeRef root_node() const;
        Document    clone() const;
        /**
         * @brief Set the root object
         * 
         * @return The old root node
         */
        Node    set_root(Node &&);

        u8string to_string(bool format = true) const {
            xmlChar *text;
            int      size;
            xmlDocDumpFormatMemory(doc,&text,&size,format);
            u8string us(reinterpret_cast<char_t *>(text),size);
            xmlFree(text);
            return us;
        }
        u8string version() const {
            return (const char_t*)doc->version;
        }

        xmlDocPtr get() const noexcept {
            return doc;
        }
    private:
        xmlDocPtr doc = nullptr;
    friend class Document;
};
/**
 * @brief Dom document
 * 
 */
class Document : public DocumentRef {
    public:
        Document() = default;
        Document(const Document &) = delete;
        Document(Document &&);
        ~Document(){
            xmlFreeDoc(doc);
        }

        explicit Document(xmlDocPtr p) : DocumentRef(p) {}

        void      assign(Document &&);
        xmlDocPtr detach() noexcept;
};


//--DomNode & Helper
class NodeChildIterator {
    public:
        NodeChildIterator() = default;
        NodeChildIterator(xmlNode *prev,xmlNode *cur) : cur(cur), prev(prev) {}
        NodeChildIterator(const NodeChildIterator &) = default;
        NodeChildIterator(NodeChildIterator &&) = default;
        ~NodeChildIterator() = default;

        //Compare
        bool operator ==(const NodeChildIterator &iter) const noexcept {
            return cur == iter.cur;
        }
        bool operator !=(const NodeChildIterator &iter) const noexcept {
            return cur != iter.cur;
        }
        //Move
        NodeChildIterator &operator ++() noexcept {
            prev = cur;
            cur = cur->next;
            return *this;
        }
        NodeChildIterator operator ++(int) noexcept {
            NodeChildIterator tmp(*this);
            ++(*this);
            return tmp;
        }
        NodeChildIterator &operator --() noexcept {
            cur = prev;
            prev = cur->prev;
            return *this;
        }
        NodeChildIterator operator --(int) noexcept {
            NodeChildIterator tmp(*this);
            --(*this);
            return tmp;
        }
        //Dereference
        NodeRef operator  *() const noexcept;
        NodeRef operator ->() const noexcept;
    private:
        xmlNode *cur = nullptr;//< Current 
        xmlNode *prev = nullptr;//< Previous
};
class NodeChildren {
    public:
        using iterator = NodeChildIterator;
        using const_iterator = NodeChildIterator;
        using value_type = NodeRef;

        NodeChildren(xmlNode *parent) {
            first = parent->children;
            last = parent->last;
        }
        NodeChildren(xmlNode *first, xmlNode *last) : first(first), last(last) {}

        iterator begin() const {
            return iterator(nullptr,first);
        }
        iterator end() const {
            return iterator(last,nullptr);
        }

    private:
        xmlNode *first = nullptr;
        xmlNode *last = nullptr;
};
// class NodeAttributes {
    
// };

/**
 * @brief The reference to a DomNode
 */
class NodeRef {
    public:
        NodeRef() = default;
        NodeRef(xmlNodePtr p) : node(p) {}
        NodeRef(const NodeRef &) = default;
        NodeRef(NodeRef &&) = default;
        ~NodeRef() = default;

        NodeRef &operator =(const NodeRef &) = default;
        NodeRef &operator =(NodeRef &&) = default;
        /**
         * @brief Check is node is null
         * 
         * @return true 
         * @return false 
         */
        bool operator ==(std::nullptr_t) const noexcept {
            return node == nullptr;
        }
        bool operator !=(std::nullptr_t) const noexcept {
            return node != nullptr;
        }
        //For iterator etc...
        NodeRef *operator ->() noexcept {
            return this;
        }
        const NodeRef *operator ->() const noexcept {
            return this;
        }
    public:
        bool is_null() const {
            return node == nullptr;
        }
        bool is_text() const {
            return xmlNodeIsText(node);
        }
        bool is_comment() const {
            return node->type == XML_COMMENT_NODE;
        }
        bool is_element() const {
            return node->type == XML_ELEMENT_NODE;
        }
        bool is_document() const {
            return node->type == XML_DOCUMENT_NODE;
        }
        bool is_blank() const {
            return xmlIsBlankNode(node) == 1;
        }
        //--Content
        u8string value() const {
            return ToString(static_cast<const xmlChar *>(node->content));
        }
        u8string content() const {
            return ToString(xmlNodeGetContent(node));
        }
        void set_content(u8string_view s) {
            xmlNodeSetContentLen(node,BAD_CAST s.data(),s.size());
        }
        void add_content(u8string_view s) {
            xmlNodeAddContentLen(node,BAD_CAST s.data(),s.size());
        }
        //--Name
        u8string name() const {
            return ToString(node->name);
        }
        void set_name(u8string_view s) {
            xmlNodeSetName(node,BAD_CAST s.data());
        }
        //--Attributes
        u8string attribute(u8string_view name) const {
            return ToString(xmlGetProp(node,BAD_CAST name.data()));
        }
        void set_attribute(u8string_view name,u8string_view value) {
            xmlSetProp(node,BAD_CAST name.data(),BAD_CAST value.data());
        }
        void remove_attribute(u8string_view name) {
            xmlUnsetProp(node,BAD_CAST name.data());
        }
        //--Parent
        NodeRef parent() const {
            return NodeRef(node->parent);
        }
        //--Children
        NodeChildren children() const {
            return NodeChildren(node->children,node->last);
        }
        NodeRef first_child() const {
            return NodeRef(node->children);
        }
        NodeRef last_child() const {
            return NodeRef(node->last);
        }
        NodeRef next_sibling() const {
            return NodeRef(node->next);
        }
        NodeRef previous_sibling() const {
            return NodeRef(node->prev);
        }
        //--Document
        DocumentRef document() const {
            return DocumentRef(node->doc);
        }
        //--Get
        xmlNodePtr get() const noexcept {
            return node;
        }
        //--Find Node
        XPathObject xpath(u8string_view s) const;
        //--Create element as child
        NodeRef create_element(u8string_view name) const;        

        Node clone() const;
    private:
        xmlNodePtr node = nullptr;
    friend class Node;
};
/**
 * @brief DomNode but, it has the ownership of the xmlNodePtr
 * 
 */
class Node : public NodeRef {
    public:
        Node() = default;
        Node(const Node &) = delete;
        Node(Node && n) {
            node = n.node;
            n.node = nullptr;
        }
        ~Node(){
            xmlFreeNode(node);
        }

        explicit Node(xmlNodePtr p) : NodeRef(p) {}

        void assign(Node &&node) {
            xmlFreeNode(this->node);
            this->node = node.node;
            node.node = nullptr;
        }
        /**
         * @brief Release the ownship
         * 
         * @return xmlNodePtr 
         */
        xmlNodePtr detach() noexcept {
            auto n = node;
            node = nullptr;
            return n;
        }

        //--Operators
        Node &operator =(Node &&node) {
            if(this != &node){
                assign(std::move(node));
            }
            return *this;
        }

        //--Create
        static Node New(const char *name) {
            return Node(xmlNewNode(nullptr,BAD_CAST name));
        }
};


class XmlDocument : public Document {
    public:
        using Document::Document;
        //--Parse a document from a string
        static XmlDocument Parse(u8string_view str,int opt = DefaultOptions);
        static XmlDocument New(const char *version = "1.0");
};

class HtmlDocument : public Document {
    public:
        using Document::Document;
        //--Parse a document from a string
        static HtmlDocument Parse(u8string_view str,int opt = DefaultOptions);
        static HtmlDocument New(const char *url = nullptr,const char *ext_id = nullptr);
};

//--Impl Node & Helper
inline NodeRef NodeChildIterator::operator *() const noexcept {
    return NodeRef(cur);
}
inline NodeRef NodeChildIterator::operator ->() const noexcept {
    return NodeRef(cur);
} 
//--Impl Doc
inline NodeRef DocumentRef::root_node() const {
    return NodeRef(xmlDocGetRootElement(doc));
}
inline Node DocumentRef::set_root(Node &&n){
    return Node(xmlDocSetRootElement(doc,n.detach()));
}

inline XmlDocument XmlDocument::Parse(u8string_view str,int opt) {
    xmlDocPtr doc = xmlReadMemory(str.data(),str.size(),"","UTF-8",opt);
#ifndef LXML_NO_EXCEPTIONS
    if(doc == nullptr){
        LXML_THROW(std::runtime_error("Failed to parse xml document"));
    }
#endif
    return XmlDocument(doc);
}
inline XmlDocument XmlDocument::New(const char *version) {
    return XmlDocument(xmlNewDoc(BAD_CAST version));
}
inline HtmlDocument HtmlDocument::Parse(u8string_view str,int opt) {
    xmlDocPtr doc = htmlReadMemory(str.data(),str.size(),"","UTF-8",opt);
#ifndef LXML_NO_EXCEPTIONS
    if(doc == nullptr){
        LXML_THROW(std::runtime_error("Failed to parse html document"));
    }
#endif
    return HtmlDocument(doc);
}
inline HtmlDocument HtmlDocument::New(const char *url,const char *ext_id) {
    return HtmlDocument(htmlNewDoc(BAD_CAST url,BAD_CAST ext_id));
}
//--Init / Quit
inline void Init() {
    xmlInitGlobals();
}
inline void Quit() {
    xmlCleanupParser();
    xmlCleanupGlobals();
    xmlDictCleanup();
}
class Library {
    public:
        Library() {
            Init();
        }
        Library(const Library &) = delete;
        ~Library() {
            Quit();
        }
};

/**
 * @brief Get the Error String
 * 
 * @return u8string 
 */
inline u8string GetError() {
    auto err = xmlGetLastError();
    if(err == nullptr){
        return u8string();
    }
    return u8string(reinterpret_cast<const char_t *>(err->message));
}
LXML_NS_END

//--XPath
LXML_NS_BEGIN
/**
 * @brief Iterator for XPathNodeSet
 * 
 */
class XPathIterator {
    public:
        XPathIterator() = default;
        XPathIterator(xmlNodeSetPtr set,int index) : set(set) , index(index) {}
        XPathIterator(const XPathIterator &) = default;
        ~XPathIterator() = default;

        XPathIterator & operator ++() {
            ++index;
            return *this;
        }
        XPathIterator & operator --() {
            --index;
            return *this;
        }
        XPathIterator operator ++(int) {
            XPathIterator it(*this);
            ++index;
            return it;
        }
        XPathIterator operator --(int) {
            XPathIterator it(*this);
            --index;
            return it;
        }

        bool operator ==(const XPathIterator & it) const noexcept {
            return index == it.index && set == it.set;
        }
        bool operator !=(const XPathIterator & it) const noexcept {
            return index != it.index || set != it.set;
        }

        NodeRef operator *() const {
            LXML_CHECK(index >= 0 && index < set->nodeNr);
            return NodeRef(set->nodeTab[index]);
        }
        NodeRef operator ->() const {
            LXML_CHECK(index >= 0 && index < set->nodeNr);
            return NodeRef(set->nodeTab[index]);
        }
        std::ptrdiff_t operator -(const XPathIterator & it) const noexcept {
            return index - it.index;
        }
        XPathIterator &operator +=(std::ptrdiff_t n) {
            index += n;
            return *this;
        }
        XPathIterator &operator -=(std::ptrdiff_t n) {
            index -= n;
            return *this;
        }
        XPathIterator operator +(std::ptrdiff_t n) const {
            XPathIterator it(*this);
            it.index += n;
            return it;
        }
        XPathIterator operator -(std::ptrdiff_t n) const {
            XPathIterator it(*this);
            it.index -= n;
            return it;
        }
    private:
        xmlNodeSetPtr set = nullptr;
        int index = 0;
};
class XPathNodeSet {
    public:
        XPathNodeSet();
        XPathNodeSet(const XPathNodeSet &) = default;
        XPathNodeSet(XPathNodeSet &&) = default;
        ~XPathNodeSet() = default;

        explicit XPathNodeSet(xmlNodeSetPtr set) : set(set) {}

        size_t size() const {
            return xmlXPathNodeSetGetLength(set);
        }
        size_t length() const {
            return xmlXPathNodeSetGetLength(set);
        }
        NodeRef at(size_t i) const {
            return NodeRef(xmlXPathNodeSetItem(set,i));
        }
        NodeRef operator [](size_t i) const {
            return NodeRef(xmlXPathNodeSetItem(set,i));
        }

        XPathIterator begin() const {
            return XPathIterator(set,0);
        }
        XPathIterator end() const {
            return XPathIterator(set,set->nodeNr);
        }
    private:
        xmlNodeSetPtr set = nullptr;
};
/**
 * @brief The result of an XPath evaluation
 * 
 */
class XPathObject {
    public:
        XPathObject();
        XPathObject(const XPathObject & other){
            obj = xmlXPathObjectCopy(other.obj);
        }
        XPathObject(XPathObject && other) {
            obj = other.obj;
            other.obj = nullptr;
        }
        ~XPathObject(){
            xmlXPathFreeObject(obj);
        }
        
        explicit XPathObject(xmlXPathObjectPtr obj) : obj(obj) {};

        bool is_null() const {
            return obj == nullptr;
        }
        bool is_nodeset() const {
            return obj->type == XPATH_NODESET;
        }
        bool is_boolean() const {
            return obj->type == XPATH_BOOLEAN;
        }
        bool is_number() const {
            return obj->type == XPATH_NUMBER;
        }
        bool is_string() const {
            return obj->type == XPATH_STRING;
        }
        
        //--Cast
        bool         as_boolean() const {
            LXML_CHECK(is_boolean());
            return xmlXPathCastToBoolean(obj);
        }
        double       as_number() const {
            LXML_CHECK(is_number());
            return xmlXPathCastToNumber(obj);
        }
        u8string     as_string() const {
            LXML_CHECK(is_string());
            return (char_t*)xmlXPathCastToString(obj);
        }
        XPathNodeSet as_nodeset() const {
            LXML_CHECK(is_nodeset());
            return XPathNodeSet(obj->nodesetval);
        }
        //--Use like iterable
        XPathIterator begin() const {
            return XPathIterator(obj->nodesetval,0);
        }
        XPathIterator end() const {
            auto set = obj->nodesetval;
            if(!is_nodeset()){
                //Set end to the first element
                return XPathIterator(set,0);
            }
            return XPathIterator(set,set->nodeNr);
        }
    private:
        xmlXPathObjectPtr obj = nullptr;
};
class XPathContent {
    public:
        XPathContent();
        XPathContent(DocumentRef doc) {
            ctxt = xmlXPathNewContext(doc.get());
        }
        XPathContent(const XPathContent &) = delete;
        XPathContent(XPathContent && other) {
            ctxt = other.ctxt;
            other.ctxt = nullptr;
        }
        ~XPathContent() {
            xmlXPathFreeContext(ctxt);
        }

        XPathExpression compile(u8string_view s);
        /**
         * @brief Eval on the current context
         * 
         * @param s Zero terminated string
         * @return XPathObject 
         */
        XPathObject eval(u8string_view         s) const {
            return XPathObject(
                xmlXPathEvalExpression(
                    BAD_CAST s.data(),
                    ctxt
                )
            );
        }
        // XPathObject eval(const XPathExpression &) const;
        /**
         * @brief Eval on node you gived,not on the root node of the document
         * 
         * @param s Zero terminated string
         * @return XPathObject 
         */
        XPathObject eval(NodeRef node,u8string_view         s) const {
            return XPathObject(
                xmlXPathNodeEval(
                    node.get(),
                    BAD_CAST s.data(),
                    ctxt
                )
            );
        }
        // XPathObject eval(NodeRef node,const XPathExpression &) const;

        //Assign
        void assign(XPathContent &&other){
            xmlXPathFreeContext(ctxt);
            ctxt = other.ctxt;
            other.ctxt = nullptr;
        }

        XPathContent &operator =(XPathContent &&other){
            if(this == &other){
                return *this;
            }
            assign(std::move(other));
            return *this;
        }
    private:
        xmlXPathContextPtr ctxt = nullptr;
};
/**
 * @brief XPath Expression on ctxt
 * 
 */
class XPathExpression {
    public:
        XPathExpression();
        XPathExpression(const XPathExpression &) = delete;
        XPathExpression(XPathExpression &&);
        ~XPathExpression() {
            xmlXPathFreeCompExpr(expr);
        }

        explicit XPathExpression(xmlXPathCompExprPtr p) : expr(p) {}
    private:
        xmlXPathCompExprPtr expr = nullptr;
};

//--Impl for find operations for NodeRef

XPathObject NodeRef::xpath(u8string_view s) const {
    XPathContent ctxt(document());
    return ctxt.eval(*this,s);
}

LXML_NS_END