#!/usr/bin/python

from Tkinter import *
import tkMessageBox
from collections import deque
import sys
import StringIO
from random import randint

class StatusBar(Frame):
	def __init__(self, parent):
		Frame.__init__(self, parent)
		self.label = Label(self, relief = SUNKEN, anchor = W)
		self.label.pack(fill = X)
	
	def set(self, format, *args):
		self.label.config(text = format % args)
		self.label.update_idletasks()
	
	def clear(self):
		self.label.config(text = "")
		self.label.update_idletasks()

class Block(object):
	def __init__(self, id, (x, y)):
		self.id = id
		self.x = x
		self.y = y

	def coord(self):
		return (self.x, self.y)


class Board(Frame):
	def __init__(self, parent, 
			procs,
			nround,
			width = 300,
			height = 300,
			roundbase = 0,
			x_space = 4,
			y_space = 4,
			x_margin = 10,
			y_margin = 10,
			x_op = 40,
			y_op = 20,
			y_label = 20
		):

		Frame.__init__(self, parent, width=width, height=height)

		nthread = 0

		thread_mapping = dict()
		for pid in procs:
			for tid in procs[pid]:
				idstr = '%d:%d' % (pid, tid)
				thread_mapping[idstr] = nthread
				nthread = nthread + 1

		scrollheight = 2 * y_margin + nround * (y_space + y_op) - y_space + y_label
		scrollwidth = 2 * x_margin + nthread * (x_space + x_op) - x_space


		self.canvas = Canvas(self, 
			height = height,
			width = width,
			scrollregion=(0, 0, scrollwidth, scrollheight)
			)
		self.xscroll = Scrollbar(self, orient='horizontal', command=self.canvas.xview)
		self.yscroll = Scrollbar(self, orient='vertical', command=self.canvas.yview)

		self.canvas["xscrollcommand"] = self.xscroll.set
		self.canvas["yscrollcommand"] = self.yscroll.set

		self.xscroll.pack(side = BOTTOM, fill=X)
		self.yscroll.pack(side = RIGHT, fill = Y)
		self.canvas.pack(side = LEFT, fill = BOTH, expand = 1)

		self.procs = procs
		self.thread_mapping = thread_mapping
		self.nthread = nthread
		self.nround = nround
		self.x_space = x_space
		self.y_space = y_space
		self.x_margin = x_margin
		self.y_margin = y_margin
		self.x_op = x_op
		self.y_op = y_op
		self.x_label = x_op
		self.y_label = y_label

		self.roundbase = roundbase
		self.parent = parent

		self.thread_colors = ['red', 'blue', 'purple']

		self.create_labels()
		self.create_separators()
		self.canvas.yview_moveto(1.0)

	def get_column(self, op):
		assert 'tid' in op and 'pid' in op
		idstr = '%d:%d' % (op['pid'], op['tid'])
		return self.thread_mapping[idstr]

	def create_separator(self, p1, q1, p2, q2, dash = None):
		(x0, y0) = self.get_pos_base(p1, q1)
		(x1, y1) = self.get_pos_base(p2, q2)
		x0 = x0 - self.x_space / 2
		y0 = y0 - self.y_space / 2
		x1 = x1 - self.x_space / 2
		y1 = y1 - self.y_space / 2
		self.canvas.create_line(x0, y0, x1, y1, dash = dash)

	def create_separators(self):
		self.create_separator(0, -1, 0, self.nround)
		self.create_separator(0, -1, self.nthread, -1)
		self.create_separator(0, 0, self.nthread, 0)
		self.create_separator(0, self.nround, self.nthread, self.nround)
		base = 0
		for proc in self.procs:
			n = len(self.procs[proc])
			for i in range(1, n):
				self.create_separator(base + i, -1, base + i, self.nround, 
					dash = (2, 4))
			base = base + n
			self.create_separator(base, -1, base, self.nround)

	def create_labels(self):

		#	draw thread labels
		for idstr in self.thread_mapping:
			i = self.thread_mapping[idstr]
			x = self.x_margin + i * (self.x_op + self.x_space) + self.x_op / 2
			y = self.y_margin + self.y_label / 2
			self.canvas.create_text(x, y, text = idstr)

		pass

	def get_pos_base(self, x, y):
		return (self.x_margin + x * (self.x_op + self.x_space),
			self.y_margin + y * (self.y_op + self.y_space) + self.y_label
			)

	def get_cpos_base(self, x, y):
		return (self.x_margin + x * (self.x_op + self.x_space) + self.x_op / 2,
			self.y_margin + y * (self.y_op + self.y_space) 
			+ self.y_op / 2 + self.y_label
			)

	def get_pos(self, op):
		thread = self.get_column(op)
		nround = op['round']
		assert(thread != None and nround != None)
		nround = nround - self.roundbase
		return self.get_pos_base(thread, nround)

	def get_cpos(self, op):
		thread = self.get_column(op)
		nround = op['round']
		assert(thread != None and nround != None)
		nround = nround - self.roundbase
		return self.get_cpos_base(thread, nround)

	def add_op(self, op):
		if not self.onboard(op):
			return None
		thread = self.get_column(op)
		assert(thread != None)

		ncolor = len(self.thread_colors)
		color = self.thread_colors[thread % ncolor]
		if ('op' not in op or op['op'] == 'unknown'):
			(rx, ry) = self.get_pos(op)
			return self.canvas.create_rectangle(rx, ry, 
				rx + self.x_op, ry + self.y_op, fill = color)
		else:
			optype = op['op']
			(rx, ry) = self.get_cpos(op)
			return self.canvas.create_text(rx, ry, text = optype, fill = color)
			

	def pointonboard(self, x, y):
		return x >= 0 and x < self.nthread and y >= self.roundbase and y < self.nround + self.roundbase

	def onboard(self, op):
		nround = op['round']
		return nround >= self.roundbase and nround < self.roundbase + self.nround

	def draw_line(self, op_from, op_to):
		if not self.onboard(op_from) or not self.onboard(op_to):
			return
		(x1, y1) = self.get_cpos(op_from)
		(x2, y2) = self.get_cpos(op_to)
		return self.canvas.create_line(x1, y1, x2, y2, 
			width = 2, arrow = 'last'
			)

#	def move_block(self, id, coord):
#		"""
#			notice here coord is relative coordinates. 
#		"""
#		x, y = coord
#		self.canvas.move(id, x * self.scale, y * self.scale)

#	def delete_block(self, id):
#		self.canvas.delete(id)

class SchedPrinter(object):
	"""
		SchedPrinter reads schedule from standard input, and visualize it. 
		Here's the input format:
		first line		: (# ops) (# deps)
		next #op lines	: (id) (pid) (tid) (op) (round) [info]
		next #deps line : (id_from) (id_to) [info]
	"""
	def __init__(self, parent, fin = sys.stdin):
		self.parent = parent 
		self.status_bar = StatusBar(parent)
		self.status_bar.pack(side = TOP, fill = X)
		self.status_bar.set('Score: %-7d' % (0))
		self.score = 0

		self.parse_input(fin)

		self.paint()
	
	def paint(self):
		self.board = Board(self.parent, self.procs, 
			min(self.nround, 10000))
		self.board.pack(fill = BOTH, side = BOTTOM, expand = 1)

		#print self.board.width, self.board.height
		#print dir(self.parent)
		#print parent.width, parent.height

		for op in self.ops.values():
			self.board.add_op(op)

		for dep in self.deps:
			(id_from, id_to, msg) = dep
			op_from = self.ops[id_from]
			op_to = self.ops[id_to]
			self.board.draw_line(op_from, op_to)
	
	def parse_input(self, fin):	
		line = fin.readline()
		(nop, ndep) = line.split()
		nop = int(nop)
		ndep = int(ndep)

		ops = dict()
		procs = dict()
		deps = []
		max_round = 0

		while (nop > 0):
			nop = nop - 1
			line = fin.readline()
			tokens = line.split()
			assert len(tokens) >= 5
			(opid, pid, tid, optype, nround) = tokens[0:5]

			msg = ' '.join(tokens[5:])
			pid = int(pid)
			tid = int(tid)
			nround = int(nround)
			op = dict()
			op['pid'] = pid
			op['tid'] = tid
			op['op'] = optype
			op['info'] = msg
			op['round'] = nround

			if nround > max_round and nround <= 100000:
				max_round = nround

			if not pid in procs:
				procs[pid] = set()
			procs[pid].add(tid)
			ops[opid] = op

		while (ndep > 0):
			ndep = ndep - 1
			line = fin.readline()
			tokens = line.split()
			assert len(tokens) >= 2
			(id_from, id_to) = tokens[0:2]
			msg = ' '.join(tokens[2:])
			assert(id_from in ops and id_to in ops)
			deps.append((id_from, id_to, msg))

		self.deps = deps
		self.ops = ops
		self.procs = procs
		self.nround = max_round + 1

def testBoard(root):
	board = Board(root, {0 : [0], 1 : [0, 1]}, 4)
	op0 = {'pid' : 0, 'tid' : 0, 'round' : 0}
	op1 = {'pid' : 1, 'tid' : 0, 'round' : 2, 'op' : 'mlock'}
	op2 = {'pid' : 1, 'tid' : 1, 'round' : 3}
	board.add_op(op0)
	board.add_op(op1)
	board.add_op(op2)
	board.draw_line(op1, op2)

def testPrinter(root):
	fin = StringIO.StringIO(
"""3 1
0 0 0 unknown 0 first
1 1 0 ML 2 second
2 1 1 ML 3 third
1 2 it deps
""")
	printer = SchedPrinter(root, fin)

def runFromStdin(root):
	printer = SchedPrinter(root, sys.stdin)

def launchSchedPaint():
	root = Tk()
	root.title('Schedule')

	#testBoard(root)
	#testPrinter(root)
	runFromStdin(root)

	root.mainloop()

if __name__ == '__main__':
  launchSchedPaint()
