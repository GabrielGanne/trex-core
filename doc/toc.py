import json
from html.parser import HTMLParser

class CTocNode:
    def __init__ (self, link_fmt = None):
        self.name="root"
        self.level=1; # 1,2,3,4
        self.link=None;
        self.parent=None
        self.childs=[]; # link to CTocNode

        # by default, link_fmt will formatted as in-page links
        self.link_fmt = link_fmt if link_fmt else lambda x: '#' + x

    def get_link (self):
        if self.link==None:
            name=self.name
            l=name.split('.');
            l=l[-1].lower()
            s='';
            for c in l:
                if c.isalpha() or c.isspace():
                    s+=c

            link = '_'.join(s.lower().split());
        else:
            link = self.link

        return self.link_fmt(link)



    def add_new_child (self,name,level,link):
        n=CTocNode();
        n.name=name
        n.link=link
        n.level=level
        n.parent=self
        n.link_fmt=n.parent.link_fmt
        self.childs.append(n);
        return n

    def to_json_childs (self):
        l=[]
        for obj in self.childs:
            l.append(obj.to_json());
        return (l);

    def to_open (self):
        if self.level <3:
            return True
        else:
            return False


    def to_json (self):
        d={"text" : self.name,
           "link" : self.get_link(),
           "state"       : {
                "opened"    : self.to_open()
                }
          }
        if len(self.childs)>0 :
            d["children"]= self.to_json_childs()
        return d

class TocHTMLParser(HTMLParser):

    def __init__ (self, link_fmt = None):
        HTMLParser.__init__(self);
        self.state=0
        self.root = CTocNode(link_fmt)
        self.root.parent=self.root
        self.level=2;
        self.attrs=None
        self.d={};
        self.last_level=1
        self.set_level(1,self.root)


    def set_level (self,level,node):
        assert(node!=None);
        assert(isinstance(node,CTocNode)==True);
        self.d[str(level)]=node

        # in case we change from high to low level remove the higher level
        if level<self.last_level:
            for l in range(level+1,self.last_level+1):
                self.d.pop(str(l),None)



    def _get_level (self,level):
        k=str(level)
        if k in self.d:
            n=self.d[k]
            assert(n!=None);
            return n
        else:
            return None

    def get_level (self,level):
        for l in range(level,0,-1):
            n=self._get_level(l)
            if n != None:
                return n
        assert(0);


    def is_header (self,tag):
        if len(tag)==2 and tag[0]=='h' and tag[1].isdigit() and (int(tag[1])>1):
            return (True);

    def handle_starttag(self, tag, attrs):
        if self.is_header (tag):
            self.attrs=attrs
            self.state=True;
            self.level=int(tag[1]);

    def handle_endtag(self, tag):
        if self.is_header (tag):
            self.state=False;

    def get_id (self):
        if self.attrs:
            for obj in self.attrs:
                if obj[0]=='id':
                    return obj[1]
        else:
            return None


    def handle_data(self, data):
        if self.state:

           level=self.level

           cnode=self.get_level(level-1)

           n=cnode.add_new_child(data,level,self.get_id());
           assert(n!=None);
           self.set_level(level,n)
           self.last_level=level

    def dump_as_json (self):
        return json.dumps(self.root.to_json_childs(), sort_keys=False, indent=4)

def create_toc_json(input_file, output_file, link_fmt=None):
    with open(input_file, encoding="utf-8") as f:
        l = f.readlines()
    html_input = ''.join(l)
    parser = TocHTMLParser(link_fmt)
    parser.feed(html_input);
    with open(output_file, 'w') as f:
        f.write(parser.dump_as_json())
