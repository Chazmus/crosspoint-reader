-- Chess Daily Puzzle Lua Application for CrossPoint Reader

local pieces = {}
local json = nil

-- Board state: 64 squares (0 = a1, 7 = h1, 56 = a8, 63 = h8)
local board = {}
local selectedSq = -1
local hintSq = -1
local statusMsg = "Welcome to Chess Puzzles"
local puzzleRating = 1500
local puzzleThemes = "tactics"
local solution = {}
local moveIndex = 1
local isPlayerWhite = true
local isSolved = false

local function initModules()
    if not json then
        local jsonSrc = storage.readFile("json.lua")
        if jsonSrc then
            local chunk = load(jsonSrc)
            if chunk then json = chunk() end
        end
    end
    if not pieces or not pieces["P"] then
        local pSrc = storage.readFile("pieces.lua")
        if pSrc then
            local chunk = load(pSrc)
            if chunk then pieces = chunk() end
        end
    end
end

local function squareToFileRank(sq)
    local f = sq % 8
    local r = math.floor(sq / 8)
    return f, r
end

local function squareToNotation(sq)
    local f, r = squareToFileRank(sq)
    local fileChar = string.char(97 + f)
    local rankChar = string.char(49 + r)
    return fileChar .. rankChar
end

local function notationToSquare(notStr)
    if not notStr or #notStr < 2 then return -1 end
    local f = string.byte(notStr, 1) - 97
    local r = string.byte(notStr, 2) - 49
    if f < 0 or f > 7 or r < 0 or r > 7 then return -1 end
    return r * 8 + f
end

local function clearBoard()
    for i = 0, 63 do
        board[i] = "."
    end
end

local function loadFen(fen)
    clearBoard()
    if not fen then return end
    local parts = {}
    for p in fen:gmatch("%S+") do table.insert(parts, p) end
    local boardPart = parts[1] or ""

    local rank = 7
    local file = 0
    for i = 1, #boardPart do
        local c = boardPart:sub(i, i)
        if c == '/' then
            rank = rank - 1
            file = 0
        elseif c:match("%d") then
            file = file + tonumber(c)
        else
            if file < 8 and rank >= 0 then
                board[rank * 8 + file] = c
                file = file + 1
            end
        end
    end
end

local function applyMove(moveStr)
    if not moveStr or #moveStr < 4 then return false end
    local fromSq = notationToSquare(moveStr:sub(1, 2))
    local toSq = notationToSquare(moveStr:sub(3, 4))
    if fromSq < 0 or toSq < 0 then return false end

    local piece = board[fromSq]
    board[fromSq] = "."
    -- Handle pawn promotion if move has 5 characters (e.g. e7e8q)
    if #moveStr >= 5 then
        local prom = moveStr:sub(5, 5)
        board[toSq] = (piece == piece:upper()) and prom:upper() or prom:lower()
    else
        board[toSq] = piece
    end
    return true
end

local function loadPuzzle()
    initModules()
    local data = storage.readFile("daily.json")
    if not data or not json then
        loadFen("r1bqkb1r/pppp1ppp/2n5/4p3/2B1n3/5N2/PPPP1PPP/RNBQK2R w KQkq - 0 5")
        statusMsg = "Sample puzzle loaded"
        return
    end

    local parsed = json.decode(data)
    if parsed and parsed.puzzle then
        puzzleRating = parsed.puzzle.rating or 1500
        puzzleThemes = (parsed.puzzle.themes and table.concat(parsed.puzzle.themes, ", ")) or "tactics"
        solution = parsed.puzzle.solution or {}
        moveIndex = 1
        isSolved = false

        -- Set starting position
        loadFen("8/8/1K6/1k6/1P6/8/8/8 w - - 0 1")

        if #solution > 0 then
            statusMsg = "Daily Puzzle (" .. tostring(puzzleRating) .. ") - Your turn!"
        end
    else
        loadFen("rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1")
    end
end

function onEnter()
    loadPuzzle()
end

function onTouch(x, y)
    local w = gfx.getWidth()
    local h = gfx.getHeight()

    -- Sleep toggle button (bottom right)
    local sleepBtnX = w - 170
    local sleepBtnY = h - 55
    if x >= sleepBtnX and x <= sleepBtnX + 150 and y >= sleepBtnY and y <= sleepBtnY + 45 then
        local current = crosspoint.getSleepApp()
        if current == "chess" then
            crosspoint.clearSleepApp()
        else
            crosspoint.setSleepApp("chess")
        end
        crosspoint.requestUpdate()
        return
    end

    -- Reload / Fetch button (bottom right - 2nd)
    local fetchBtnX = w - 320
    local fetchBtnY = h - 55
    if x >= fetchBtnX and x <= fetchBtnX + 130 and y >= fetchBtnY and y <= fetchBtnY + 45 then
        if crosspoint.isWifiConnected() then
            statusMsg = "Fetching daily puzzle from Lichess..."
            crosspoint.requestUpdate()
            local online = crosspoint.httpGet("https://lichess.org/api/puzzle/daily")
            if online and online ~= "" then
                storage.writeFile("daily.json", online)
                loadPuzzle()
                statusMsg = "New puzzle downloaded!"
            else
                statusMsg = "Failed to download puzzle"
            end
        else
            statusMsg = "Wi-Fi not connected"
        end
        crosspoint.requestUpdate()
        return
    end

    -- Check chessboard tap
    local boardSize = 400
    local boardX = 20
    local boardY = 40
    local sqSize = boardSize / 8

    if x >= boardX and x < boardX + boardSize and y >= boardY and y < boardY + boardSize then
        local dispFile = math.floor((x - boardX) / sqSize)
        local dispRank = math.floor((y - boardY) / sqSize)
        local file = dispFile
        local rank = 7 - dispRank
        local sq = rank * 8 + file

        if selectedSq == -1 then
            if board[sq] ~= "." then
                selectedSq = sq
                crosspoint.requestUpdate()
            end
        else
            if sq == selectedSq then
                selectedSq = -1
                crosspoint.requestUpdate()
            else
                -- Attempt move
                local moveStr = squareToNotation(selectedSq) .. squareToNotation(sq)
                selectedSq = -1

                if #solution >= moveIndex and moveStr == solution[moveIndex] then
                    applyMove(moveStr)
                    moveIndex = moveIndex + 1
                    if moveIndex > #solution then
                        isSolved = true
                        statusMsg = "Puzzle Solved! Excellent!"
                    else
                        -- Opponent response
                        local reply = solution[moveIndex]
                        applyMove(reply)
                        moveIndex = moveIndex + 1
                        if moveIndex > #solution then
                            isSolved = true
                            statusMsg = "Puzzle Solved!"
                        else
                            statusMsg = "Correct! Continue..."
                        end
                    end
                else
                    applyMove(moveStr) -- Free play / demo move
                    statusMsg = "Move played: " .. moveStr
                end
                crosspoint.requestUpdate()
            end
        end
    end
end

function onInput(button, isDown)
    if button == input.BTN_CONFIRM then
        loadPuzzle()
        crosspoint.requestUpdate()
    end
end

local function drawChessBoard(boardX, boardY, boardSize)
    local sqSize = boardSize / 8
    local pieceOffset = (sqSize - 40) / 2

    gfx.drawRect(boardX - 2, boardY - 2, boardSize + 4, boardSize + 4, 2, true)

    for rank = 0, 7 do
        for file = 0, 7 do
            local dispFile = file
            local dispRank = 7 - rank
            local sqX = boardX + dispFile * sqSize
            local sqY = boardY + dispRank * sqSize
            local sq = rank * 8 + file

            local isDark = ((file + rank) % 2 == 0)
            if isDark then
                gfx.fillRectDither(sqX, sqY, sqSize, sqSize, gfx.COLOR_LIGHT_GRAY)
            else
                gfx.fillRect(sqX, sqY, sqSize, sqSize, false)
            end

            -- Selection highlight
            if sq == selectedSq then
                gfx.drawRect(sqX + 1, sqY + 1, sqSize - 2, sqSize - 2, 3, true)
            end

            -- Draw piece
            local p = board[sq]
            if p and p ~= "." and pieces[p] then
                gfx.drawSprite(sqX + pieceOffset, sqY + pieceOffset, 40, 40, pieces[p].ink, pieces[p].mask)
            end
        end
    end
end

function onDraw()
    local w = gfx.getWidth()
    local h = gfx.getHeight()

    gfx.clearScreen(1)

    -- Top bar
    gfx.fillRect(0, 0, w, 32, true)
    gfx.drawText(gfx.FONT_UI_10, 20, 22, "Chess Puzzles (Lichess)", false)

    -- Draw Board (400x400)
    drawChessBoard(20, 42, 400)

    -- Side info panel
    local panelX = 440
    gfx.drawText(gfx.FONT_UI_12, panelX, 70, "Daily Tactics", true)
    gfx.drawLine(panelX, 85, w - 20, 85, 1, true)

    gfx.drawText(gfx.FONT_SMALL, panelX, 115, "Rating: " .. tostring(puzzleRating), true)
    gfx.drawText(gfx.FONT_SMALL, panelX, 140, "Themes: " .. puzzleThemes, true)

    -- Status message box
    gfx.drawRoundedRect(panelX, 170, w - panelX - 20, 60, 6, 2, true)
    gfx.drawText(gfx.FONT_SMALL, panelX + 10, 205, statusMsg, true)

    -- Buttons
    local fetchBtnX = w - 320
    local fetchBtnY = h - 55
    gfx.drawRoundedRect(fetchBtnX, fetchBtnY, 130, 45, 8, 2, true)
    gfx.drawText(gfx.FONT_SMALL, fetchBtnX + 24, fetchBtnY + 28, "Update", true)

    local sleepBtnX = w - 170
    local sleepBtnY = h - 55
    local isSleepActive = (crosspoint.getSleepApp() == "chess")

    if isSleepActive then
        gfx.fillRoundedRect(sleepBtnX, sleepBtnY, 150, 45, 8, gfx.COLOR_BLACK)
        gfx.drawText(gfx.FONT_SMALL, sleepBtnX + 28, sleepBtnY + 28, "Sleep: ON", false)
    else
        gfx.drawRoundedRect(sleepBtnX, sleepBtnY, 150, 45, 8, 2, true)
        gfx.drawText(gfx.FONT_SMALL, sleepBtnX + 24, sleepBtnY + 28, "Set Sleep", true)
    end
end

function onSleepDraw()
    local w = gfx.getWidth()
    local h = gfx.getHeight()

    initModules()
    loadPuzzle()

    gfx.clearScreen(1)
    gfx.fillRect(0, 0, w, 40, true)
    gfx.drawCenteredText(gfx.FONT_UI_10, 26, "DAILY CHESS PUZZLE", false)

    -- Draw board in center/left
    local boardSize = 360
    local boardX = 25
    local boardY = 60
    drawChessBoard(boardX, boardY, boardSize)

    -- Sleep information panel
    local infoX = 410
    gfx.drawText(gfx.FONT_UI_12, infoX, 90, "Daily Tactics", true)
    gfx.drawLine(infoX, 105, w - 25, 105, 1, true)

    gfx.drawText(gfx.FONT_SMALL, infoX, 140, "Rating: " .. tostring(puzzleRating), true)
    gfx.drawText(gfx.FONT_SMALL, infoX, 170, "Themes: " .. puzzleThemes, true)
    gfx.drawText(gfx.FONT_SMALL, infoX, 210, "Turn: " .. (isPlayerWhite and "White" or "Black") .. " to play", true)

    gfx.drawCenteredText(gfx.FONT_SMALL, h - 25, "Press Power to Wake and Solve", true)
end
