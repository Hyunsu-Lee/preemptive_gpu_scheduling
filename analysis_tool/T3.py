import sys
from antlr4 import *
import antlr4
from antlr4.TokenStreamRewriter import TokenStreamRewriter
from antlr4.Token import CommonToken

from CPP14Lexer import CPP14Lexer
from CPP14Listener import CPP14Listener
from CPP14Parser import CPP14Parser
from enum import Enum

class BasicInfo():
	def __init__(self):
		self.name = None
		self.indi_types = None
		self.data_type = None
		self.ptr_type = None
		self.line = None
	
	def setName(self, name):
		self.name = name
	def setIndiType(self, itypes):
		self.indi_types = itypes

	def setType(self, dtype):
		self.data_type = dtype
	
	def setPtrType(self, nr_ptr, is_ref):
		self.ptr_type = (nr_ptr, is_ref)
	
	def setLine(self, line):
		self.line = line

class UseInfo():
	def __init__(self, name):
		self.name = name
		self.rwc = dict()
		self.inner_ptr = dict()
		self.index = None
	
	def add_iptr(self, parent, iptr):
		parent.update({iptr:UseInfo()})
	
	def search_iptr(self, name):
		return self.inner_ptr.get(name)


class ValueInfo(BasicInfo):
	def __init__(self):
		self.nr_pointer = 0
		self.inner_ptr = dict()
		self.relation = dict()

	def add_use(self, ui):
		self.uses.update({ui.name:ui})
	
	def search_use(self, name):
		return self.uses.get(name)
	
	def add_iptr(self, parent, iptr):
		parent.update({iptr:UseInfo()})
	
	def search_iptr(self, name):
		return self.inner_ptr.get(name)


class FunctionInfo(BasicInfo):
	
	def __init__(self):
		self.nr_param = 0
		self.params = dict()
		self.nr_alias = 0
		self.aliases = dict()

	def add_param(self, param):
		self.params.update({param.name:param})
		self.nr_param+=1
	
	def add_alias(self, alias):
		self.aliases.update({alias.name:alias})
		self.nr_alias+=1
	
	def search_param(self, name):
		return self.params.get(name)

	def search_alias(self, name):
		return self.aliases.get(name)

	def search_value(self, name):
		value = self.params.get(name)
		if value == None:
			value = self.aliases.get(name)
		return value

	def rwc_print(self, rwc):
		for i in rwc.keys():
			for j in rwc[i].keys():
				print("->RWC(%d ,%d): %s (index: %s)"%(i, j, rwc[i][j][0], rwc[i][j][1]))
	def relation_print(self, rel):
		for i in rel.keys():
			print("->Rel(%d): %s"%(i, rel[i]))
	def value_print(self, iptr, depth):#dict
		if len(iptr.keys()):
			print("iptr keys(%d): %s"%(depth, iptr.keys()))
		depth+=1
		for ptr in iptr.keys():
			print("Current IPTR: %s"%ptr)
			self.rwc_print(iptr[ptr].rwc)
			self.value_print(iptr[ptr].inner_ptr, depth)

	

class StaticAnalysisInfo():
	functions = dict()

	#전역 변수 추적을 위해 동일하게 추가할 것
	
	def add_function(self, fi):
		self.functions.update({fi.name:fi})
	def search_function(self, name):
		return self.functions.get(name)
	

'''
Tracker1 :
	해당 클래스는 1차적으로 소스 코드내에 함수들을 추적하고,
	매개변수로 전달된 글로벌 메모리 변수들의 정보 추출 및 사용되는 라인과 목적을 추적 한다.
'''
class Tracker1(CPP14Listener):

	#Function Definition
	def __init__(self, sai, stream):
		self.sai = sai # Call by Reference : StaticAnalysisInfo
		self.stream = stream
		self.rewrite = TokenStreamRewriter(stream)
		self.fi = None
		
		#Function
		self.func_flag = False
		self.fbody_flag = False
		self.gpu_func = False
		
		self.equal_flag = False
		self.first_left = False
		self.first_right = False
		self.rw_equal_flag = False
		self.prev_value = None

		self.postfix_count = 0
		self.alias_init = None

		self.postfix_call_count = 0
		self.call_list = list()
		
		self.jump_flag = False
		self.selstatcond = False

	def FuncInfoTrack(self, ctx, info):

		ret = None
		ptr_count = 0
		
		ptr = ctx.getChild(1)
		while not(type(ptr) == CPP14Parser.PtrdeclaratorContext):
			if type(ptr) == CPP14Parser.InitdeclaratorContext and ptr.getChildCount() == 2:
				ret = ptr.getChild(1) #InitializerContext
			ptr = ptr.getChild(0)

		while type(ptr.getChild(0)) == CPP14Parser.PtroperatorContext:
			ptr_count+=1
			ptr = ptr.getChild(1)
		
		noptr = ptr.getChild(0)

		if noptr.getChildCount() == 3:
			ptr_count+=1
		
		if ptr.parentCtx.getChild(0).getText() =='&': #Reference Check
			info.setPtrType(ptr_count, 1)
		else:
			info.setPtrType(ptr_count, 0)

		while not(type(noptr.getChild(0)) == CPP14Parser.UnqualifiedidContext):
			noptr = noptr.getChild(0)

		itypes = list()
		ret_type = ctx.getChild(0)
		while ret_type.getChildCount() > 1:
			itypes.append(ret_type.getChild(0).getText())
			ret_type = ret_type.getChild(1)

		info.setLine(ctx.start.line)
		info.setName(noptr.getText())
		info.setType(ret_type.getText())
		info.setIndiType(itypes)

		return ret
	
	def FuncInfoPrint(self, fi):
		print("---------------------------------")
		print("Function Start Line <%d>"%fi.line)
		print("Function Name: %s"%fi.name)
		print("Function Type: %s"%fi.indi_types)
		print("Function Returen: %s %s"%(fi.data_type, fi.ptr_type))
		print("Function Parameters:")
		for param in fi.params:
			p = fi.params[param]
			print("%s %s %s"%(p.data_type, p.name, p.ptr_type))

	def enterFunctiondefinition(self, ctx):
		#Tracking function name
		self.func_flag = True
		self.gpu_func = False
		
		self.fi = FunctionInfo()
		self.FuncInfoTrack(ctx, self.fi)
		
		#tmp
		#self.rewrite.insertAfter(ctx.start, "test")
		
	def enterParameterdeclaration(self, ctx):
		if self.func_flag == True and self.fbody_flag == False:
			vi = ValueInfo()
			self.FuncInfoTrack(ctx, vi)
			if vi.ptr_type[0] > 0:
				self.fi.add_param(vi)
	
	def exitParametersandqualifiers(self, ctx):
		if self.func_flag == True and self.fbody_flag == False:
			for itype in self.fi.indi_types:
				if itype == '__device__' or itype == '__global__':
					self.gpu_func = True
					self.sai.add_function(self.fi)
					self.FuncInfoPrint(self.fi)
				
	def enterFunctionbody(self, ctx):
		self.fbody_flag = True
	
	def exitFunctionbody(self, ctx):
		self.fbody_flag = False
	
	def exitFunctiondefinition(self, ctx):
		self.func_flag = False

		print("-------------------------")
		for v in self.fi.params.keys():
			print("Value Info(param) : %s"%v)
			self.fi.value_print(self.fi.params[v].inner_ptr, 0)
			print("-------------------------")
		
		for v in self.fi.aliases.keys():
			print("Value Info(alias) : %s"%v)
			self.fi.relation_print(self.fi.aliases[v].relation)
			self.fi.value_print(self.fi.aliases[v].inner_ptr, 0)
			print("-------------------------")

##############################################################################################

	def enterIterationstatement(self, ctx):
		pass
		# for/ while 정보 초기화(중첩 정보)
	def enterSelectionstatement(self, ctx):
		pass
		# if 정보 초기화(중첩 정보)
	def enterCondition(self, ctx):
		self.selstatcond = True
	def exitCondition(self, ctx):
		self.selstatcond = False

	def enterJumpstatement(self, ctx):
		self.jump_flag = True
	def exitJumpstatement(self, ctx):
		self.jump_flag = False
		
	def enterDeclarationstatement(self, ctx):
		if self.fbody_flag == True and self.gpu_func == True:
			sDec = ctx.getChild(0).getChild(0)
			vi = ValueInfo()

			init = self.FuncInfoTrack(sDec, vi)
			if vi.ptr_type[0] > 0:
				self.fi.add_alias(vi)
				#
				self.alias_init = vi.name
				print("alias: %s"%vi.name);
					
			
			#if not(init == None):
			#	self.equal_flag = True
			#	print("De-init: %s"%init.getText())
				'''
				여기서는 해당 변수에 삽입되는 변수가 기존에 선언 되었거나, 
				'''
			#print("------(D)%d: %s ------%s"%(ctx.start.line, ctx.getText(), sDec.getChild(0, CPP14Parser.DeclspecifierContext)))
			#print("%s %s %s"%(vi.data_type, vi.name, vi.ptr_type))

	def enterExpressionstatement(self, ctx):
		if self.fbody_flag == True and self.gpu_func == True:
			self.first_left = False
			self.first_right = False
			self.rw_equal_flag = False
			#print("------(E)%d: %s"%(ctx.start.line, ctx.getText()))
	
	def enterBraceorequalinitializer(self, ctx):
		if self.fbody_flag == True and self.gpu_func == True:
			if ctx.getChildCount()>1 and ctx.getChild(0).getText() == '=':
				self.equal_flag = True
				#print("Braceo")

	def exitBraceorequalinitializer(self, ctx):
		if self.fbody_flag == True and self.gpu_func == True:
			if ctx.getChildCount()>1 and ctx.getChild(0).getText() == '=':
				self.equal_flag = False


	def enterAssignmentexpression(self, ctx):
		if self.fbody_flag == True and self.gpu_func == True:
			if ctx.getChildCount() > 2:
				if not(ctx.getChild(1).getText() == "="):
					self.rw_equal_flag = True
		
	def enterAssignmentoperator(self, ctx):
		if self.fbody_flag == True and self.gpu_func == True:
			self.equal_flag = True
			
	def exitAssignmentexpression(self, ctx):
		if self.fbody_flag == True and self.gpu_func == True:
			if ctx.getChildCount() > 2:
				self.equal_flag = False

	def enterPostfixexpression(self, ctx):
		if self.fbody_flag == True and self.gpu_func == True:
			if ctx.getChildCount() > 1:
				self.postfix_count+=1
			#print("--------------POST: %s, %d"%(ctx.getText(), ctx.start.tokenIndex))
			rwc = None
			if self.equal_flag == False:
				if self.jump_flag or self.selstatcond:
					rwc = "r"
				elif self.rw_equal_flag == True:
					self.rw_equal_flag = False
					rwc = "rw"
				else:
					rwc = "w"
			elif self.equal_flag == True:	
				rwc = "r"
				if self.first_right == False:
					if self.rw_equal_flag == True:
						self.rw_equal_flag = False
						rwc = "rw"
			if self.postfix_call_count:
				rwc = "c"
			#print("--%s(rwc)[%d]: %s(%s)"%(ctx.getText(), ctx.start.line, rwc, self.equal_flag))
				
				#print("(%d) %d, %d: %s: %s"%(self.postfix_call_count, ctx.start.line, ctx.start.tokenIndex, ctx.getText(), rwc))

			if ctx.getChildCount() == 3:
				ptr_list = ctx.getText().split('.')
				value = self.fi.search_value(ptr_list[0])
				
				if not value == None:
					parent = value
					for ptr in ptr_list:
						if not(parent.inner_ptr.get(ptr)):
							ui = UseInfo(ptr)
							parent.inner_ptr.update({ptr:ui})

						#print("pf: %s->%s"%(parent.name, parent.inner_ptr[ptr].name))
						
						parent = parent.inner_ptr[ptr]
					
					if not(parent.rwc.get(ctx.start.line)):
						parent.rwc.update({ctx.start.line: dict()})
					if not(parent.rwc[ctx.start.line].get(ctx.start.tokenIndex)):
						parent.rwc[ctx.start.line].update({ctx.start.tokenIndex: [None, None]})
					
					parent.rwc[ctx.start.line][ctx.start.tokenIndex][0] = rwc
					
					if rwc == "c":
						parent.rwc[ctx.start.line][ctx.start.tokenIndex][1] = self.call_list[-1]

			if ctx.getChildCount() == 4:

				if type(ctx.getChild(2)) == CPP14Parser.ExpressionlistContext:
					self.postfix_call_count+=1
					self.call_list.append(ctx.getText().split('(')[0])

				ptr_list = ctx.getText().split('[')
				value_name = ptr_list[0]
				value = self.fi.search_value(value_name)
				if not value == None:
					ui = UseInfo(value_name)
					if not(value.inner_ptr.get(value_name)):
						value.inner_ptr.update({value_name:ui})
					
					parent = value.inner_ptr[value_name]

					if not(parent.rwc.get(ctx.start.line)):
						parent.rwc.update({ctx.start.line: dict()})
					if not(parent.rwc[ctx.start.line].get(ctx.start.tokenIndex)):
						parent.rwc[ctx.start.line].update({ctx.start.tokenIndex: [None, None]})
					parent.rwc[ctx.start.line][ctx.start.tokenIndex][0] = rwc
					if rwc == "c":
						parent.rwc[ctx.start.line][ctx.start.tokenIndex][1] = self.call_list[-1]
					else:
						parent.rwc[ctx.start.line][ctx.start.tokenIndex][1] = ctx.getChild(2).getText()

					#value.inner_ptr[value_name].index = ctx.getChild(2).getText()

			elif ctx.getChildCount() == 1:
				value = self.fi.search_value(ctx.getText())
				if not value == None:
					ui = UseInfo(ctx.getText())
					if not(value.inner_ptr.get(ctx.getText())):
						value.inner_ptr.update({ctx.getText():ui})
						#print("11111111111: %s"%value.inner_ptr)
					
					parent = value.inner_ptr[ctx.getText()]
					
					if not(parent.rwc.get(ctx.start.line)):
						parent.rwc.update({ctx.start.line: dict()})
					if not(parent.rwc[ctx.start.line].get(ctx.start.tokenIndex)):
						parent.rwc[ctx.start.line].update({ctx.start.tokenIndex: [None, None]})
					if not(parent.rwc[ctx.start.line][ctx.start.tokenIndex][0] == "rw"):
						parent.rwc[ctx.start.line][ctx.start.tokenIndex][0] = rwc
					if rwc == "c":
						parent.rwc[ctx.start.line][ctx.start.tokenIndex][1] = self.call_list[-1]


	def exitPostfixexpression(self, ctx):
		if self.fbody_flag == True and self.gpu_func == True:
			if ctx.getChildCount() > 1:
				self.postfix_count-=1
			
			if ctx.getChildCount() == 4:
				if type(ctx.getChild(2)) == CPP14Parser.ExpressionlistContext:
					self.postfix_call_count-=1
					self.call_list.pop()

			if self.postfix_count == 0:
				value = ctx.getText()
				info = ctx.getText()

				if ctx.getChildCount() <= 3:
					ptr_list = ctx.getText().split('.')
					value = ptr_list[0]
				elif ctx.getChildCount() == 4:
					value = ctx.getText().split('[')[0]
				

				#Relationship
				if self.equal_flag == False:
					self.alias_init = value
					#print("alias init(%d): %s"%(ctx.start.line, self.alias_init))
				elif self.equal_flag == True:	
					if self.fi.search_value(self.alias_init):
						relation = self.fi.search_value(self.alias_init).relation
						if not(relation.get(ctx.start.line)):
							relation.update({ctx.start.line:list()})

						tmp_value = self.fi.search_value(value)
						if tmp_value:
							if tmp_value.relation.keys():
								key = list(tmp_value.relation.keys())[-1]
								value = tmp_value.relation[key]

							relation[ctx.start.line].append(value)

				#print("Last Post Fix : %s->%s"%(ctx.getText(), value))



	def enterUnqualifiedid(self, ctx):
		#help(ctx)
		#help(self.stream)
		print("%d, %d: %s %s"%(ctx.start.tokenIndex, ctx.start.tokenIndex, ctx.getText(), ctx.start.text))
		ctx.start.text = "change"
		print("%d, %d: %s %s"%(ctx.start.tokenIndex, ctx.start.tokenIndex, ctx.getText(), ctx.start.text))
		if self.fbody_flag == True and self.gpu_func == True:
			#Use

			if self.equal_flag == False:
				if self.first_left == False:
					self.first_left = True
					#print("f-left(%d): %s"%(ctx.start.line, ctx.getText()))
				else:
					pass
					#print("left(%d): %s"%(ctx.start.line, ctx.getText()))

			elif self.equal_flag == True:	
				if self.first_right == False:
					if self.rw_equal_flag == True:
						self.rw_equal_flag = False
						#print("rw-left(%d): %s"%(ctx.start.line, self.prev_value))
					#print("f-right(%d): %s"%(ctx.start.line, ctx.getText()))
				else:
					pass
					#print("right(%d): %s"%(ctx.start.line, ctx.getText()))

				#init state recognization
				'''
				이것을 하기 위해서는 여기서 하면 안되고, initializer 컨텍스트 이후에 진행 되어야 한다. 
				왜냐하면, int *tmp = global + tmp[threadblock] 
				형태로 되어 있다면 여기서는 threadblock 까지 인지하기 때문이다.
				하필이면 만약 threadblock 이것이 단순 변수가 아닌 포인터였다면, 이쪽에서 검색할 경우 이미 등록된 것이기 떄문에
				실상 관련 없음에도 불구하고 관련 있는 것으로 검색될 수 있다.
				'''
			else:
				print("non-spec(%d): %s"%(ctx.start.line, ctx.getText()))

			self.prev_value = ctx.getText()
	
	def exitStatement(self, ctx):
		pass
		#self.equal_flag = False



def main(argv):
	sai = StaticAnalysisInfo()

	input = FileStream(argv[1])
	lexer = CPP14Lexer(input)
	stream = CommonTokenStream(lexer)
	parser = CPP14Parser(stream)
	tree = parser.translationunit()
	
	tracker  = Tracker1(sai, stream)
	walker = ParseTreeWalker()

	walker.walk(tracker, tree)


	print(stream.getText())
	stream.tokens[0].text = "test"
	tmp_tok = stream.getTokens(0,3)
	space = CommonToken()
	space.text = ' '
	tmp_tok.append(space)
	stream.tokens[2:2] = tmp_tok
	#help(stream.tokens[0])

	#tmp_tok = stream.tokens[2].clone()
	#stream.tokens.insert(2, tmp_tok)
	#ttt = CommonToken()
	#ttt.text = '???'
	#stream.tokens.insert(2, ttt)


	print(stream.getText())
	print("---------")
	#lll = stream.getTokens(0,5)
	#print(lll)
	#print(stream.tokens[0].getTokenSource().text)

	#print(tracker.rewrite.getTokenStream().getText())
	'''
	print(len(tokens.tokens)-1)
	print(tracker.rewrite.tokens)
	print("-----------------------")
	print(tokens)
	print("-----------------------")
	print(tokens.tokens)
	print("-----------------------")
	for t in range(len(tokens.tokens)):

		print(t)
		print(tokens.tokens[t])
		print(tokens.tokens[t].text)
		print("")
		#print(t.getText())
	
	print(tracker.rewrite.getProgram("default"))
	print(tracker.rewrite.getText("default", range(0, len(tokens.tokens)-1)))
	'''
	#print(tracker.rewrite.getText("default", range(0, 2)))
	#print(tokens.getText())
	#help(tracker.rewrite)
	#help(parser)



if __name__ == '__main__':
	main(sys.argv)
