import version
import config
import enum
import utils
import json
import  re

__author__ = 'Matteo Marescotti'


class SolveStatus(enum.Enum):
    unknown = 0
    sat = 1
    unsat = -1

class SplitPreference(enum.Enum):
    sppref_tterm_neq = 'tterm_neq'
    sppref_eq = 'eq'
    sppref_noteq = 'noteq'
    sppref_tterm = 'tterm'
    sppref_blind = 'blind'
    sppref_bterm = 'bterm'

class Node:
    def __init__(self, parent, smt):
        if parent and not isinstance(parent, Node):
            raise TypeError('Node expected')
        self._parent = parent
        if self._parent is not None:
            self._parent._children.append(self)
            self._root = self._parent._root
        else:
            self._root = self
        self.smt = smt
        self._children = []
        self._status = SolveStatus.unknown
        self._started = None
        self._assumed_timout = False
        self.assumed_timout_r = False
        self._partitioning = False

    def __repr__(self):
        return '<{}:{}>'.format(
            self.path(),
            self.status.name
        )

    def __hash__(self):
        return id(self)

    def __lt__(self, other):
        return len(self.path()) < len(other.path())

    def __eq__(self, other):
        return self is other

    def __ne__(self, other):
        return self is not other

    def __getitem__(self, key):
        return self._children[key]

    def __iter__(self):
        return self._children.__iter__()

    def __len__(self):
        # print(self.path())
        return self._children.__len__()

    def path(self, nodes=False):
        node = self
        path = []
        while node.parent:
            path.append(node.parent if nodes else node.parent._children.index(node))
            node = node.parent
        path.reverse()
        return path

    def is_ancestor(self, node):
        if self.root != node.root:
            raise ValueError('provided node from a different tree')
        selfp = self.path()
        nodep = node.path()
        if len(selfp) > len(nodep):
            return False
        for i in range(len(selfp)):
            if selfp[i] != nodep[i]:
                return False
        return True

    def all(self):
        nodes = [self]
        for node in self:
            nodes += node.all()
        return nodes

    def clear(self):
        self._children.clear()

    @property
    def parent(self):
        return self._parent

    @property
    def root(self):
        return self._root

    @property
    def level(self):
        return len(self.path())

    @property
    def status(self):
        return self._status

    @status.setter
    def status(self, status):
        self._set_status(status)

    def _set_status(self, status: SolveStatus):
        raise NotImplementedError

    @property
    def started(self):
        return self._started

    @started.setter
    def started(self, time):
        self._started = time

    @property
    def assumed_timout(self):
        return self._assumed_timout

    @assumed_timout.setter
    def assumed_timout(self, asked):
        self._assumed_timout = asked

    @property
    def assumed_timout_r(self):
        return self._assumed_timout_r

    @assumed_timout_r.setter
    def assumed_timout_r(self, asked):
        self._assumed_timout_r = asked

    @property
    def partitioning(self):
        return self._partitioning

    @partitioning.setter
    def partitioning(self, asked):
        self._partitioning = asked

class AndNode(Node):
    def __init__(self, parent, smt, expected_time_point=0):
        if not isinstance(parent, (OrNode, type(None))):
            raise TypeError
        super().__init__(parent, smt)
        self._processed = False
        self._is_timeout = False
        self._expected_timeout_point = expected_time_point
        # print("_expected_timeout_point", expected_time_point)

    def _set_status(self, status: SolveStatus):
        self._status = status
        if self.parent:
            if status == SolveStatus.sat or all([node.status == status for node in self.parent]):
                self.parent.status = status

    def childeren(self):
        childs = []
        for ch in (self._children[0])._children:
            childs.append(ch)
        return childs

    @property
    def processed(self):
        return self._processed

    @processed.setter
    def processed(self, p):
        self._processed = p

    @property
    def is_timeout(self):
        return self._is_timeout

    @is_timeout.setter
    def is_timeout(self, it):
        self._is_timeout = it

    @property
    def expected_timeout_point(self):
        return self._expected_timeout_point

    @expected_timeout_point.setter
    def expected_timeout_point(self, etp):
        self._expected_timeout_point = etp

class OrNode(Node):
    def __init__(self, parent, smt: str = ''):
        if not isinstance(parent, AndNode):
            raise TypeError
        super().__init__(parent, smt)

    def _set_status(self, status: SolveStatus):
        self._status = status
        self.parent.status = status


class Root(AndNode):
    def __init__(self, name: str, smt: str):
        super().__init__(None, smt, config.node_timeout)
        self.name = name

    def __repr__(self):
        return '<{} "{}":{}>'.format(
            self.__class__.__name__,
            self.name,
            self.status.name
        )

    def child(self, path: list):
        node = self
        for i in path:
            node = node[int(i)]
        return node

    def to_string(self, node: AndNode, start: AndNode = None):
        raise NotImplementedError


class SMT(Root):
    def __init__(self, name: str, smt: str):
        super().__init__(name, (re.sub(r';(.*)(\n+)', '', smt)).split('(check-sat)')[0])

    def to_string(self, node: AndNode, start: AndNode = None):
        n_pop = 0
        if start is not None:
            while not start.is_ancestor(node):
                if isinstance(start, AndNode):
                    n_pop += 1
                start = start.parent
        smt = []

        while node is not start:
            if isinstance(node, AndNode):
                smt.append(node.smt)
                if node != self.root:
                    smt.append('(push 1)')
            node = node.parent
        smt += ['(pop 1)' for _ in range(n_pop)]
        smt.reverse()
        return '\n'.join(smt), '(check-sat)'


class Fixedpoint(Root):
    def __init__(self, name: str, smt: str):
        self.json = utils.smt2json(smt)
        self.query = None
        for i in self.json:
            if i[0] == 'query':
                self.query = i[1]
                self.json = self.json[:self.json.index(i)]
                break
        if self.query is None:
            raise ValueError('query not found')
        self.json = [i for i in self.json if i[0] != 'query']
        super().__init__(name, smt.split('(query ')[0])

    def partition(self):
        obj = self.json.copy()
        queries = []
        for i in obj:
            try:
                if i[0] != 'rule':
                    continue
                i = i[1]
                while i[0] == 'let':
                    i = i[2]
                if i[0] == '=>' and i[2] == self.query:
                    queries.append('{}{}'.format(self.query, len(queries)))
                    i[2] = queries[-1]
            except:
                pass

        for i in range(len(obj)):
            if obj[i][0] == 'declare-rel':
                for query in queries:
                    obj.insert(i, ['declare-rel', query, []])
                break

        parent = OrNode(self, utils.json2smt(obj))
        if config.db():
            config.db().cursor().execute("INSERT INTO {}SolvingHistory (name, node, event, solver, data) "
                                         "VALUES (?,?,?,?,?)".format(config.table_prefix), (
                                             self.name,
                                             str(self.path()),
                                             'OR',
                                             '',
                                             json.dumps({'node': str(parent.path())})
                                         ))

        for query in queries:
            child = AndNode(parent, '(query ' + query + ')')
            if config.db():
                config.db().cursor().execute("INSERT INTO {}SolvingHistory (name, node, event, solver, data) "
                                             "VALUES (?,?,?,?,?)".format(config.table_prefix), (
                                                 self.name,
                                                 str(self.path()),
                                                 'AND',
                                                 '',
                                                 json.dumps({'node': str(child.path())})
                                             ))

        if config.db():
            config.db().commit()

    def to_string(self, node: AndNode, start: AndNode = None):
        self.is_ancestor(node)
        if start is not None:
            raise ValueError
        if node is self:
            return self.smt, '(query ' + self.query + ')'
        elif isinstance(node, AndNode):
            return node.parent.smt, node.smt


# class Fixedpoint(Root):
#     def __init__(self, name: str, fixedpoint, queries):
#         s = fixedpoint.to_string([])
#         self.query = str(queries[0].sexpr())
#         super().__init__(name, s[s.index('(declare-rel '):])
#
#     def partition(self, fixedpoint, queries):
#         if len(self) > 0:
#             raise ValueError('already partitioned')
#         _f = config.z3().Fixedpoint(ctx=fixedpoint.ctx)
#         query = queries[0]
#         queries = []
#         for rule in fixedpoint.get_rules():
#             if config.z3().is_quantifier(rule):
#                 imp = rule.body()
#                 body = imp.arg(0)
#                 if imp.num_args() == 2:  # if is implies
#                     head = imp.arg(1)
#                     if config.z3().is_and(body):
#                         for i in range(body.num_args()):  # app always before others
#                             ch = body.arg(i)
#                             if config.z3().is_app(ch) and ch.decl().kind() == config.z3().Z3_OP_UNINTERPRETED:
#                                 _f.register_relation(ch.decl())
#                             else:
#                                 break
#                     else:
#                         _f.register_relation(body.decl())
#                     if head.eq(query):
#                         head = config.z3().Bool(query.sexpr() + str(len(queries)), fixedpoint.ctx)
#                         queries.append(head)
#                     _f.register_relation(head.decl())
#                     _f.add_rule(head, body)
#                     continue
#             else:
#                 imp = rule
#             _f.register_relation(imp.decl())
#             _f.add_rule(rule)
#
#         s = _f.to_string([])
#         parent = OrNode(self, s[s.index('(declare-rel '):])
#         if config.db():
#             config.db().cursor().execute("INSERT INTO {}SolvingHistory (name, node, event, solver, data) "
#                                          "VALUES (?,?,?,?,?)".format(config.table_prefix), (
#                                              self.name,
#                                              str(self.path()),
#                                              'OR',
#                                              '',
#                                              json.dumps({'node': str(parent.path())})
#                                          ))
#
#         for query in queries:
#             child = AndNode(parent, '(query ' + query.sexpr() + ')')
#             if config.db():
#                 config.db().cursor().execute("INSERT INTO {}SolvingHistory (name, node, event, solver, data) "
#                                              "VALUES (?,?,?,?,?)".format(config.table_prefix), (
#                                                  self.name,
#                                                  str(self.path()),
#                                                  'AND',
#                                                  '',
#                                                  json.dumps({'node': str(child.path())})
#                                              ))
#
#         if config.db():
#             config.db().commit()
#
#     def to_string(self, node: AndNode, start: AndNode = None):
#         if node.root != self:
#             raise ValueError
#         if node is self:
#             return self.smt, '(query ' + self.query + ')'
#         elif isinstance(node, AndNode):
#             return node.parent.smt, node.smt


class MCMT(Root):
    def __init__(self, name: str, smt: str):
        super().__init__(name, smt)

    def to_string(self, node: AndNode, start: AndNode = None):
        return self.smt, ''


def parse(name: str, smt: str):
    if name.endswith(".mcmt"):
        return MCMT(name, smt)
    if smt.find('(query ') > 0:
        return Fixedpoint(name, smt)
    else:
        return SMT(name, smt)
