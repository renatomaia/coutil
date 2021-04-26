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

local level = 0
local sections = { [level] = { entries = {} } }
local previous

local lineno, links = 0, {}
local contents

for line in file:lines() do
	lineno = lineno+1
	if line == "---" then
		contents = true
	elseif contents then
		local newlevel, name = line:match("^%s*(#+)%s(.-)%s*$")
		if newlevel then
			newlevel = #newlevel
			previous = nil
		else
			name = underlined("%=", line, previous)
			if name then
				newlevel = 1
				table.remove(sections[level], #sections[level])
			else
				name = underlined("%-", line, previous)
				if name then
					newlevel = 2
					table.remove(sections[level], #sections[level])
				end
			end
		end
		if name == "Index" and newlevel == 1 then
			break
		elseif name then
			local newsection = {
				title = name,
				anchor = makeanchor(name),
				empty = true,
				hidden = newlevel > 2,
				entries = {},
			}
			if previous then table.insert(newsection, previous) end
			table.insert(newsection, line)
			local funcname, arguments = name:match("`([%w%.:]+)([^`]-)`")
			if funcname then
				newsection.indexed = "`"..funcname.."`"
				if funcname:find(":", 1, true) then
					newlevel = newlevel+1
				end
			end
			if newlevel > level then
				for i = level+1, newlevel-1 do
					sections[i] = sections[level]
				end
				level = newlevel
			elseif newlevel < level then
				for i = newlevel+1, level do
					sections[i] = nil
				end
				level = newlevel
			end
			table.insert(sections[level-1].entries, newsection)
			sections[level] = newsection
		else
			for name, url in line:gmatch("%[([^%]]+)%]%(([^%)]+)%)") do
				local anchor = url:match("^#([%w%-]+)$")
				if anchor then
					table.insert(links, { anchor = anchor, line = lineno })
				end
			end
			if sections[level].empty then
				local module = line:match("^Module `([%w%.:]+)`")
				if module then
					sections[level].indexed = "`"..module.."`"
					sections[level].empty = false
				else
					sections[level].empty = not line:match("%S")
				end
			end
			table.insert(sections[level], line)
		end
	end
	previous = line
end

file:close()

local function writeanchor(text, entry, prefix)
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

local function writesummary(section, indexed, prefix)
	if section.indexed then
		table.insert(indexed, section)
		indexed[section] = {}
		indexed = indexed[section]
	end
	for i, subsection in ipairs(section.entries) do
		writeanchor(subsection.title, subsection, not subsection.hidden and prefix.."- " or nil)
		writesummary(subsection, indexed, prefix.."\t")
	end
end

local function writesection(section)
	for _, line in ipairs(section) do
		io.write(line, "\n")
	end
	for _, subsection in ipairs(section.entries) do
		writesection(subsection)
	end
end

local function sortentry(a, b)
	return (a.indexed or a.title) < (b.indexed or b.title)
end

local function isorted(t)
	local sorted = {}
	for i, v in ipairs(t) do
		sorted[i] = v
	end
	table.sort(sorted, sortentry)
	return ipairs(sorted)
end

local function writeindex(indexed, prefix)
	for _, section in isorted(indexed) do
		writeanchor(section.indexed, section, prefix.."- ")
		writeindex(indexed[section], "\t"..prefix)
	end
end

io.write[[
Summary
=======

]]
local indexed = {}
writesummary(sections[0], indexed, "")

io.write[[
- [Index](#index)

---
]]
writesection(sections[0])

for _, link in ipairs(links) do
	if links[link.anchor] == nil then
		io.stderr:write("ERROR: invalid anchor ", link.anchor, " on line ", link.line, "\n")
	end
end

io.write[[
Index
=====

]]
writeindex(indexed, "")
