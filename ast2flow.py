import json,sys,pprint
from collections import defaultdict
#flow description: storing next targets of a block 
#	

class LastJump:
	def __init__(self,srcid,how):
		self.srcid = srcid
		self.how = how
	def __repr__(self):
		return "Jump(%d,%s)" % (self.srcid,selfhow)

class Context:
	def __init__(self):
		self.loop = None
		self.loopstart = None
		self.looplasts = None
		self.functionlasts = []
		self.lasts = []

class Steps:
	def __init__(self):
		self.lasts = defaultdict(list)
		self.prev = defaultdict(list)
		self.next = defaultdict(list)
	def goto(self,froms,id,how):
		for preid in froms:
			self.prev[id].append((preid,how))
			self.next[preid].append((id,how))
	def addlast(self,lasts,target,how):
		for id in lasts:
			self.lasts[target] = LastJump(id,how)
	def getlasts(self,target):
		return self.lasts[target]

def getblocks(b,blocks):
	blocks[b["id"]] = ((b["id"],b["type"]))
	if "children" in b:
		for x in b["children"]:
			getblocks(x,blocks)


def computeflow(b,ctx,steps):
	t = b["type"]
	id = b["id"]
	descend = False
	restoreLoop = None
	if t == "function":
		# entrance -> [sequence] -> exit
		# return goes to exit
		#
		# we simulate return with goto to function
		descend = True		
		ctx.lasts = [id] 
	elif t == "if":
		# lasts -> if
		# if -> then
		# if -> else
		steps.goto(ctx.lasts,id,"")
		thenb = b["children"][0]
		ctx.lasts = [id]		
		computeflow(thenb,ctx,steps)
		if len(b["children"]) > 1:
			elseb = b["children"][1]
			thenlasts = ctx.lasts
			ctx.lasts = [id]		
			computeflow(elseb,ctx,steps)
			ctx.lasts.extend(thenlasts)
	elif t == "return":
		steps.goto(ctx.lasts,id,"") # does something
		ctx.functionlasts.extend([id]) # ids
		ctx.lasts = []
	elif t == "continue":
		steps.goto(ctx.lasts,ctx.loopstart,"continue")
		ctx.lasts = []
	elif t == "break":
		ctx.looplasts.extend(ctx.lasts) # ids
		ctx.lasts = []
	elif t == "expr":
		steps.goto(ctx.lasts,id,"")
		ctx.lasts = [id]
	elif t == "while":
		steps.goto(ctx.lasts,id,"")
		if len(b["children"]) > 0:
			firstid = b["children"][0]["id"]
			ol = ctx.loop
			ols = ctx.loopstart
			oll = ctx.looplasts
			ctx.loopstart = id
			ctx.loop = id
			ctx.looplasts = []
			ctx.lasts = [id]
			# while -> first
			for c in b["children"]:
				computeflow(c,ctx,steps)
			steps.goto(ctx.lasts,id,"loopstart") # top of while
			ctx.looplasts.append(id)
			ctx.lasts = ctx.looplasts
			ctx.loop = ol
			ctx.loopstart = ols
			ctx.looplasts = oll
		else:
			steps.goto([id],id,"cond")
			ctx.lasts = [id] # self
	elif t == "compound" or t == "capture":
		# effectively is invisible
		descend = True		
		pass
	else:
		print "unknown type",t,"for id",id
		return 
	if descend:
		# compound
		for c in b["children"]:
			computeflow(c,ctx,steps)

def main():
	if len(sys.argv) < 2:
		print 	"required filename"
		return
	a = json.loads(open(sys.argv[1],"r").read())
	# toplevel: dictionary of files with key as name
	# file: children contains array
	for filename,content in a.iteritems():
		for toplevel in content:
			if toplevel["type"] != "function":
				continue
			blocks = dict()
			bb = getblocks(toplevel,blocks)
			c = Context()
			c.function = toplevel["id"]
			steps = Steps()
			computeflow(toplevel,c,steps)
			print "prev"
			for b in blocks.values():
				if len(steps.prev[b[0]]) > 0:
					print "b:%s" % str(b),":",["%s" % str(p) for p in steps.prev[b[0]]]
			print c.functionlasts
	#print json.dumps(a,sort_keys=True,                 indent=4, separators=(',', ': '))

if __name__ == '__main__':
	main()