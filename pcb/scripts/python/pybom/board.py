#board.py

import csv
import itertools
from part import Part
from xml.etree import ElementTree

class Board:
	@staticmethod
	def FromEagle(brd_file):
		
		tree = ElementTree.parse(brd_file)
		if tree is None:
			raise ValueError("File %s is not a valid XML file" % brd_file)

		root = tree.getroot()

		elems = root.find("./drawing/board/elements")
		if elems is None:
			raise ValueError("File %s does not contain a list of elements.  This is an invalid board file" % brd_file)

		parts = list(elems)

		unknown = []
		constant = []
		variable = []
		variants = find_variants(elems)
		print "Variants Found"
		print variants

		#Create a part object for each part that has a valid digikey-pn
		#If there are multiple digikey part numbers, create a part for each one
		for part in parts:
			attribs = part.findall("attribute")
			popd = filter(lambda x: x.get('name', 'Unnamed').startswith('DIGIKEY-PN'), attribs)

			#If there are no DIGIKEY-PN attributes, this must an error or an unpopulated part
			#If there are more than one, there must be multiple assembly variants
			if len(popd) == 0:
				print "Part %s found with no part number" % part.get('name',"Unknown Name")
				unknown.append(part.get('name',"Unknown Name"))

			#See if this is a constant part (with no changes in different variants)
			p = Part.FromBoardElement(part, "")
			if p:
				constant.append(p)
			else:
				for v in variants:
					p = Part.FromBoardElement(part, v)
					if p:
						variable.append( (v, p) )

		#If only one assembly variant, give it the name Main
		if len(variants) == 0:
			return Board("test", 'test', {'MAIN': constant})
		else:
			vars = {}
			#Create multiple variants
			for var in variants:
				print "Processing Variant %s" % var
				vars[var] = constant + filter_sublists(variable, var)

			return Board("test", 'test', vars)


	def __init__(self, name, file, variants):
		"""
		Create a Board Object with 1 or more assembly variants from the variant dictionary passed in.
		"""

		self.name = name
		self.file = file
		self.variants = {varname: self._process_variant(var) for (varname,var) in variants.iteritems()}

	def _process_variant(self, parts):
		"""
		Group items by unique key and return tuples of identical parts
		"""

		bomitems = []

		sparts = sorted(parts, key=lambda x:x.unique_id())
		for k,g in itertools.groupby(sparts, lambda x:x.unique_id()):
			bomitems.append(list(g))

		return bomitems

	def list_variants(self):
		print "Assembly Variants"

		for k in self.variants.iterkeys():
			print k

	def print_variant(self, key):
		var = self.variants[key]

		print "Variant", key
		for v in var:
			print map(lambda x: x.name, v)

	def export_list(self, variant):
		"""
		Return an array with all of the digikey part numbers in this variant so that
		it can be concatenated and used to order parts
		"""

		export = []
		ignored = []

		for line in self.variants[variant]:
			pn = line[0].digipn
			if pn is None or pn == "":
				ignored.append(line[0].name)

			export += [pn]*len(line)

		print "Ignoring Parts"
		print ignored

		return export

	def export_bom(self, variant, file, title=""):
		var = self.variants[variant]

		lineno = 1

		with open(file, "wb") as bom:
			writ = csv.writer(bom)

			writ.writerow(["WellDone"])
			writ.writerow(["BOM: %s (%s)" % (title, variant)])
			writ.writerow([])
			writ.writerow([])
			writ.writerow([])

			headers = ["Item", "Qty", "Reference Design", "Value", "Footprint", "Description", "Manufacturer", "Manu. Part", "Distributor", "Dist. Part"]
			writ.writerow(headers)

			for line in var:
				num = len(line)
				refs = ", ".join(map(lambda x: x.name, line))
				foot = line[0].package
				value = line[0].value
				manu = line[0].manu
				mpn = line[0].mpn
				distpn = line[0].digipn
				dist = ""
				descr = ""
				if distpn:
					dist = "Digikey"

				row = [lineno, num, refs, value, foot, descr, manu, mpn, dist, distpn]

				writ.writerow(row)

				lineno += 1

def remove_prefix(s, prefix):
	if not s.startswith(prefix):
		return s

	return s[len(prefix):]

def filter_sublists(master, key):
	"""
	Given a list of lists where each of the sublist elements are tuples (key, value),
	return a concatenated list of all values in tuples that match key
	"""

	concat = itertools.chain(master)

	filtered = filter(lambda x: x[0] == key, concat)

	return map(lambda x:x[1], filtered)

def get_variant_id(pn_name):
	known_prefixes = ['DIGIKEY-PN']

	for p in known_prefixes:
		pn_name = remove_prefix(pn_name, p)

	if pn_name.startswith('-'):
		pn_name = pn_name[1:]

	return pn_name

def find_variants(root):
	vars = root.findall(".//attribute[@value='%s']" % 'ASSY-VARIANT')

	if len(vars) == 0:
		return ["MAIN"]
	else:
		return map(lambda x: x.get('name'), vars)