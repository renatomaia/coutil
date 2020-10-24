local path = ...

local file = assert(io.open(path))

local function makeanchor(title)
	return title:gsub("%p", ""):gsub("%s", "-"):lower()
end

local function underlined(kind, line, previous)
	local under = line:match("^"..kind:rep(3).."+$")
	if under == line and #under == #previous then
		return previous
	end
end

local current, previous, sections, section = {}
local lineno, links = 0, {}
for line in file:lines() do
	lineno = lineno+1
	if line == "^#%s*Contents%s*$"
	or underlined("=", line, previous) == "Contents" then
		sections = {}
	elseif sections then
			local name = line:match("^%s*##%s(.-)%s*$")
		if name then
			previous = nil
		else
			name = underlined("%-", line, previous)
			if name then
				table.remove(current, #current)
			end
		end
		if name then
			section = {
				title = name,
				anchor = makeanchor(name),
				empty = true,
				entries = {},
			}
			if previous then table.insert(section, previous) end
			table.insert(section, line)
			table.insert(sections, section)
			current = section
		else
			local name, url = line:match("%[([^%]]+)%]%(([^%)]+)%)")
			if name then
				local anchor = url:match("^#([%w%-]+)$")
				if anchor then
					table.insert(links, { anchor = anchor, line = lineno })
				end
			end
			if section then
				if section.empty then
					section.empty = not line:match("%S")
					local module = line:match("^Module `([%w%.:]+)`")
					if module then
						section.module = "`"..module.."`"
					end
				else
					local name, extra = line:match("### `([%w%.:]+)([^`]-)`")
					if name then
						local entry = {
							title = "`"..name.."`",
							anchor = makeanchor(name..extra),
						}
						table.insert(section.entries, entry)
						current = entry
					end
				end
			end
			table.insert(current, line)
		end
	end
	previous = line
end

file:close()

local function writeanchor(prefix, text, entry)
	local previous = links[entry.anchor]
	if previous and previous ~= entry then
		io.stderr:write("WARN : ", entry.title, " has the same anchor of ", previous.title, "\n")
	else
		links[entry.anchor] = entry
	end
	if prefix then
		io.write(prefix, "[", text, "](#", entry.anchor, ")\n")
	end
end

local function sortentry(a, b)
	return (a.module or a.title) < (b.module or b.title)
end

local function isorted(t)
	local sorted = {}
	for i, v in ipairs(t) do
		sorted[i] = v
	end
	table.sort(sorted, sortentry)
	return ipairs(sorted)
end

io.write[[

Summary
=======

]]
for index, section in ipairs(sections) do
	writeanchor(index..". ", section.title, section)
end

io.write[[

Index
=====

]]
for _, section in isorted(sections) do
	if section.module then
		writeanchor("- ", section.module, section)
		for _, entry in isorted(section.entries) do
			writeanchor("\t- ", entry.title, entry)
		end
	else
		writeanchor(nil, section.title, section)
	end
end

io.write[[

Contents
========

]]
for _, section in ipairs(sections) do
	for _, line in ipairs(section) do
		io.write(line, "\n")
	end
	for _, entry in ipairs(section.entries) do
		for _, line in ipairs(entry) do
			io.write(line, "\n")
		end
	end
end

for _, link in ipairs(links) do
	if links[link.anchor] == nil then
		io.stderr:write("ERROR: invalid anchor ", link.anchor, " on line ", link.line, "\n")
	end
end
