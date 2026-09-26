-- Tally Counter Lua Application for CrossPoint Reader

local count = 0
local dataFile = "count.txt"

local function loadCount()
    local saved = storage.readFile(dataFile)
    if saved and saved ~= "" then
        count = tonumber(saved) or 0
    end
end

function onEnter()
    loadCount()
end

local function saveCount()
    storage.writeFile(dataFile, tostring(count))
    crosspoint.requestUpdate()
end

function onTouch(x, y)
    local w = gfx.getWidth()
    local h = gfx.getHeight()

    -- Check sleep toggle button (bottom right)
    local sleepBtnX = w - 180
    local sleepBtnY = h - 60
    if x >= sleepBtnX and x <= sleepBtnX + 160 and y >= sleepBtnY and y <= sleepBtnY + 45 then
        local currentSleep = crosspoint.getSleepApp()
        if currentSleep == "counter" then
            crosspoint.clearSleepApp()
        else
            crosspoint.setSleepApp("counter")
        end
        crosspoint.requestUpdate()
        return
    end

    -- Check reset button (bottom left)
    local resetBtnX = 20
    local resetBtnY = h - 60
    if x >= resetBtnX and x <= resetBtnX + 120 and y >= resetBtnY and y <= resetBtnY + 45 then
        count = 0
        saveCount()
        return
    end

    -- Tap left side of screen for -1
    if x < w / 3 then
        count = count - 1
        saveCount()
    -- Tap right side or center for +1
    else
        count = count + 1
        saveCount()
    end
end

function onInput(button, isDown)
    if button == input.BTN_DOWN or button == input.BTN_CONFIRM or button == input.BTN_PAGE_FORWARD then
        count = count + 1
        saveCount()
    elseif button == input.BTN_UP or button == input.BTN_PAGE_BACK then
        count = count - 1
        saveCount()
    end
end

function onDraw()
    local w = gfx.getWidth()
    local h = gfx.getHeight()

    gfx.clearScreen(1)

    -- Header
    gfx.fillRect(0, 0, w, 40, true)
    local headerText = "Tally Counter"
    local hw = gfx.getTextWidth(gfx.FONT_UI_10, headerText)
    local hh = gfx.getLineHeight(gfx.FONT_UI_10)
    gfx.drawText(gfx.FONT_UI_10, math.floor((w - hw) / 2), math.floor((40 - hh) / 2), headerText, false)

    -- Subtitle
    gfx.drawCenteredText(gfx.FONT_SMALL, 60, "Tap right for +1, left for -1, or use side buttons", true)

    -- Big number box in center
    local boxW = 320
    local boxH = 160
    local boxX = math.floor((w - boxW) / 2)
    local boxY = math.floor((h - boxH) / 2 - 20)

    gfx.drawRoundedRect(boxX, boxY, boxW, boxH, 16, 3, true)
    local numStr = tostring(count)
    local numW = gfx.getTextWidth(gfx.FONT_UI_12, numStr)
    local numH = gfx.getLineHeight(gfx.FONT_UI_12)
    gfx.drawText(gfx.FONT_UI_12, boxX + math.floor((boxW - numW) / 2), boxY + math.floor((boxH - numH) / 2), numStr, true)

    -- Reset button
    local resetBtnX = 20
    local resetBtnY = h - 60
    local resetBtnW = 120
    local resetBtnH = 45
    gfx.drawRoundedRect(resetBtnX, resetBtnY, resetBtnW, resetBtnH, 8, 2, true)
    local rw = gfx.getTextWidth(gfx.FONT_SMALL, "Reset")
    local rh = gfx.getLineHeight(gfx.FONT_SMALL)
    gfx.drawText(gfx.FONT_SMALL, resetBtnX + math.floor((resetBtnW - rw) / 2), resetBtnY + math.floor((resetBtnH - rh) / 2), "Reset", true)

    -- Sleep Screen Toggle button
    local sleepBtnX = w - 180
    local sleepBtnY = h - 60
    local sleepBtnW = 160
    local sleepBtnH = 45
    local isSleepActive = (crosspoint.getSleepApp() == "counter")
    local sleepText = isSleepActive and "Sleep: ON" or "Set Sleep"
    local sw = gfx.getTextWidth(gfx.FONT_SMALL, sleepText)
    local sh = gfx.getLineHeight(gfx.FONT_SMALL)

    if isSleepActive then
        gfx.fillRoundedRect(sleepBtnX, sleepBtnY, sleepBtnW, sleepBtnH, 8, gfx.COLOR_BLACK)
        gfx.drawText(gfx.FONT_SMALL, sleepBtnX + math.floor((sleepBtnW - sw) / 2), sleepBtnY + math.floor((sleepBtnH - sh) / 2), sleepText, false)
    else
        gfx.drawRoundedRect(sleepBtnX, sleepBtnY, sleepBtnW, sleepBtnH, 8, 2, true)
        gfx.drawText(gfx.FONT_SMALL, sleepBtnX + math.floor((sleepBtnW - sw) / 2), sleepBtnY + math.floor((sleepBtnH - sh) / 2), sleepText, true)
    end
end

function onSleepDraw()
    loadCount()
    local w = gfx.getWidth()
    local h = gfx.getHeight()

    gfx.clearScreen(1)
    gfx.fillRect(0, 0, w, 50, true)
    local title = "CROSSPOINT TALLY"
    local tw = gfx.getTextWidth(gfx.FONT_UI_12, title)
    local th = gfx.getLineHeight(gfx.FONT_UI_12)
    gfx.drawText(gfx.FONT_UI_12, math.floor((w - tw) / 2), math.floor((50 - th) / 2), title, false)

    local boxW = 340
    local boxH = 180
    local boxX = math.floor((w - boxW) / 2)
    local boxY = math.floor((h - boxH) / 2 - 10)

    gfx.drawRoundedRect(boxX, boxY, boxW, boxH, 16, 4, true)
    gfx.drawCenteredText(gfx.FONT_SMALL, boxY + 25, "CURRENT COUNT", true)
    
    local numStr = tostring(count)
    local nw = gfx.getTextWidth(gfx.FONT_UI_12, numStr)
    local nh = gfx.getLineHeight(gfx.FONT_UI_12)
    gfx.drawText(gfx.FONT_UI_12, boxX + math.floor((boxW - nw) / 2), boxY + 80, numStr, true)

    gfx.drawCenteredText(gfx.FONT_SMALL, h - 35, "Press Power to Wake", true)
end
