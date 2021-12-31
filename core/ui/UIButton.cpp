/****************************************************************************
Copyright (c) 2013-2016 Chukong Technologies Inc.
Copyright (c) 2017-2018 Xiamen Yaji Software Co., Ltd.
Copyright (c) 2021 Bytedance Inc.

https://adxe.org

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
****************************************************************************/

#include "ui/UIButton.h"
#include "ui/UIScale9Sprite.h"
#include "2d/CCLabel.h"
#include "2d/CCSprite.h"
#include "2d/CCActionInterval.h"
#include "platform/CCFileUtils.h"
#include "ui/UIHelper.h"
#include <algorithm>

NS_CC_BEGIN

namespace ui
{

static const int NORMAL_RENDERER_Z       = (-2);
static const int PRESSED_RENDERER_Z      = (-2);
static const int DISABLED_RENDERER_Z     = (-2);
static const int TITLE_RENDERER_Z        = (-1);
static const float ZOOM_ACTION_TIME_STEP = 0.05f;

IMPLEMENT_CLASS_GUI_INFO(Button)

Button::Button()
    : _buttonNormalRenderer(nullptr)
    , _buttonClickedRenderer(nullptr)
    , _buttonDisabledRenderer(nullptr)
    , _titleRenderer(nullptr)
    , _zoomScale(0.1f)
    , _prevIgnoreSize(true)
    , _scale9Enabled(false)
    , _pressedActionEnabled(false)
    , _capInsetsNormal(Rect::ZERO)
    , _capInsetsPressed(Rect::ZERO)
    , _capInsetsDisabled(Rect::ZERO)
    , _normalTextureSize(_contentSize)
    , _pressedTextureSize(_contentSize)
    , _disabledTextureSize(_contentSize)
    , _normalTextureLoaded(false)
    , _pressedTextureLoaded(false)
    , _disabledTextureLoaded(false)
    , _normalTextureAdaptDirty(true)
    , _pressedTextureAdaptDirty(true)
    , _disabledTextureAdaptDirty(true)
    , _normalFileName("")
    , _clickedFileName("")
    , _disabledFileName("")
    , _normalTexType(TextureResType::LOCAL)
    , _pressedTexType(TextureResType::LOCAL)
    , _disabledTexType(TextureResType::LOCAL)
    , _fontName("")
{
    setTouchEnabled(true);
}

Button::~Button() {}

Button* Button::create()
{
    Button* widget = new Button();
    if (widget->init())
    {
        widget->autorelease();
        return widget;
    }
    CC_SAFE_DELETE(widget);
    return nullptr;
}

Button* Button::create(std::string_view normalImage,
                       std::string_view selectedImage,
                       std::string_view disableImage,
                       TextureResType texType)
{
    Button* btn = new Button;
    if (btn->init(normalImage, selectedImage, disableImage, texType))
    {
        btn->autorelease();
        return btn;
    }
    CC_SAFE_DELETE(btn);
    return nullptr;
}

bool Button::init(std::string_view normalImage,
                  std::string_view selectedImage,
                  std::string_view disableImage,
                  TextureResType texType)
{

    // invoke an overridden init() at first
    if (!Widget::init())
    {
        return false;
    }

    loadTextures(normalImage, selectedImage, disableImage, texType);

    return true;
}

bool Button::init()
{
    if (Widget::init())
    {
        return true;
    }
    return false;
}

void Button::initRenderer()
{
    _buttonNormalRenderer   = Scale9Sprite::create();
    _buttonClickedRenderer  = Scale9Sprite::create();
    _buttonDisabledRenderer = Scale9Sprite::create();
    _buttonClickedRenderer->setRenderingType(Scale9Sprite::RenderingType::SIMPLE);
    _buttonNormalRenderer->setRenderingType(Scale9Sprite::RenderingType::SIMPLE);
    _buttonDisabledRenderer->setRenderingType(Scale9Sprite::RenderingType::SIMPLE);

    addProtectedChild(_buttonNormalRenderer, NORMAL_RENDERER_Z, -1);
    addProtectedChild(_buttonClickedRenderer, PRESSED_RENDERER_Z, -1);
    addProtectedChild(_buttonDisabledRenderer, DISABLED_RENDERER_Z, -1);
}

bool Button::createTitleRendererIfNull()
{
    if (!_titleRenderer)
    {
        createTitleRenderer();
        return true;
    }

    return false;
}

void Button::createTitleRenderer()
{
    _titleRenderer = Label::create();
    _titleRenderer->setAnchorPoint(Vec2::ANCHOR_MIDDLE);
    addProtectedChild(_titleRenderer, TITLE_RENDERER_Z, -1);
}

/** replaces the current Label node with a new one */
void Button::setTitleLabel(Label* label)
{
    if (_titleRenderer != label)
    {
        CC_SAFE_RELEASE(_titleRenderer);
        _titleRenderer = label;
        CC_SAFE_RETAIN(_titleRenderer);

        addProtectedChild(_titleRenderer, TITLE_RENDERER_Z, -1);
        updateTitleLocation();
    }
}

/** returns the current Label being used */
Label* Button::getTitleLabel() const
{
    return _titleRenderer;
}

void Button::setScale9Enabled(bool able)
{
    if (_scale9Enabled == able)
    {
        return;
    }

    _scale9Enabled = able;

    if (_scale9Enabled)
    {
        _buttonNormalRenderer->setRenderingType(Scale9Sprite::RenderingType::SLICE);
        _buttonClickedRenderer->setRenderingType(Scale9Sprite::RenderingType::SLICE);
        _buttonDisabledRenderer->setRenderingType(Scale9Sprite::RenderingType::SLICE);
    }
    else
    {
        _buttonNormalRenderer->setRenderingType(Scale9Sprite::RenderingType::SIMPLE);
        _buttonClickedRenderer->setRenderingType(Scale9Sprite::RenderingType::SIMPLE);
        _buttonDisabledRenderer->setRenderingType(Scale9Sprite::RenderingType::SIMPLE);
    }

    if (_scale9Enabled)
    {
        bool ignoreBefore = _ignoreSize;
        ignoreContentAdaptWithSize(false);
        _prevIgnoreSize = ignoreBefore;
    }
    else
    {
        ignoreContentAdaptWithSize(_prevIgnoreSize);
    }

    setCapInsetsNormalRenderer(_capInsetsNormal);
    setCapInsetsPressedRenderer(_capInsetsPressed);
    setCapInsetsDisabledRenderer(_capInsetsDisabled);

    _brightStyle = BrightStyle::NONE;
    setBright(_bright);

    _normalTextureAdaptDirty   = true;
    _pressedTextureAdaptDirty  = true;
    _disabledTextureAdaptDirty = true;
}

bool Button::isScale9Enabled() const
{
    return _scale9Enabled;
}

void Button::ignoreContentAdaptWithSize(bool ignore)
{
    if (_unifySize)
    {
        this->updateContentSize();
        return;
    }

    if (!_scale9Enabled || (_scale9Enabled && !ignore))
    {
        Widget::ignoreContentAdaptWithSize(ignore);
        _prevIgnoreSize = ignore;
    }
}

void Button::loadTextures(std::string_view normal,
                          std::string_view selected,
                          std::string_view disabled,
                          TextureResType texType)
{
    loadTextureNormal(normal, texType);
    loadTexturePressed(selected, texType);
    loadTextureDisabled(disabled, texType);
}

void Button::loadTextureNormal(std::string_view normal, TextureResType texType)
{
    _normalFileName    = normal;
    _normalTexType     = texType;
    bool textureLoaded = true;
    if (normal.empty())
    {
        _buttonNormalRenderer->resetRender();
        textureLoaded = false;
    }
    else
    {
        switch (texType)
        {
        case TextureResType::LOCAL:
            _buttonNormalRenderer->initWithFile(normal);
            break;
        case TextureResType::PLIST:
            _buttonNormalRenderer->initWithSpriteFrameName(normal);
            break;
        default:
            break;
        }
    }
    // FIXME: https://github.com/cocos2d/cocos2d-x/issues/12249
    if (!_ignoreSize && _customSize.equals(Vec2::ZERO))
    {
        _customSize = _buttonNormalRenderer->getContentSize();
    }
    this->setupNormalTexture(textureLoaded);
}

void Button::setupNormalTexture(bool textureLoaded)
{
    _normalTextureSize = _buttonNormalRenderer->getContentSize();

    this->updateChildrenDisplayedRGBA();

    if (_unifySize)
    {
        if (!_scale9Enabled)
        {
            updateContentSizeWithTextureSize(this->getNormalSize());
        }
    }
    else
    {
        updateContentSizeWithTextureSize(_normalTextureSize);
    }
    _normalTextureLoaded     = textureLoaded;
    _normalTextureAdaptDirty = true;
}

void Button::loadTextureNormal(SpriteFrame* normalSpriteFrame)
{
    _buttonNormalRenderer->initWithSpriteFrame(normalSpriteFrame);
    this->setupNormalTexture(nullptr != normalSpriteFrame);
}

void Button::loadTexturePressed(std::string_view selected, TextureResType texType)
{
    _clickedFileName   = selected;
    _pressedTexType    = texType;
    bool textureLoaded = true;
    if (selected.empty())
    {
        _buttonClickedRenderer->resetRender();
        textureLoaded = false;
    }
    else
    {
        switch (texType)
        {
        case TextureResType::LOCAL:
            _buttonClickedRenderer->initWithFile(selected);
            break;
        case TextureResType::PLIST:
            _buttonClickedRenderer->initWithSpriteFrameName(selected);
            break;
        default:
            break;
        }
    }
    this->setupPressedTexture(textureLoaded);
}

void Button::setupPressedTexture(bool textureLoaded)
{
    _pressedTextureSize = _buttonClickedRenderer->getContentSize();

    this->updateChildrenDisplayedRGBA();

    _pressedTextureLoaded     = textureLoaded;
    _pressedTextureAdaptDirty = true;
}

void Button::loadTexturePressed(SpriteFrame* pressedSpriteFrame)
{
    _buttonClickedRenderer->initWithSpriteFrame(pressedSpriteFrame);
    this->setupPressedTexture(nullptr != pressedSpriteFrame);
}

void Button::loadTextureDisabled(std::string_view disabled, TextureResType texType)
{
    _disabledFileName  = disabled;
    _disabledTexType   = texType;
    bool textureLoaded = true;
    if (disabled.empty())
    {
        _buttonDisabledRenderer->resetRender();
        textureLoaded = false;
    }
    else
    {
        switch (texType)
        {
        case TextureResType::LOCAL:
            _buttonDisabledRenderer->initWithFile(disabled);
            break;
        case TextureResType::PLIST:
            _buttonDisabledRenderer->initWithSpriteFrameName(disabled);
            break;
        default:
            break;
        }
    }
    this->setupDisabledTexture(textureLoaded);
}

void Button::setupDisabledTexture(bool textureLoaded)
{
    _disabledTextureSize = _buttonDisabledRenderer->getContentSize();

    this->updateChildrenDisplayedRGBA();

    _disabledTextureLoaded     = textureLoaded;
    _disabledTextureAdaptDirty = true;
}

void Button::loadTextureDisabled(SpriteFrame* disabledSpriteFrame)
{
    _buttonDisabledRenderer->initWithSpriteFrame(disabledSpriteFrame);
    this->setupDisabledTexture(nullptr != disabledSpriteFrame);
}

void Button::setCapInsets(const Rect& capInsets)
{
    setCapInsetsNormalRenderer(capInsets);
    setCapInsetsPressedRenderer(capInsets);
    setCapInsetsDisabledRenderer(capInsets);
}

void Button::setCapInsetsNormalRenderer(const Rect& capInsets)
{
    _capInsetsNormal = Helper::restrictCapInsetRect(capInsets, this->_normalTextureSize);

    // for performance issue
    if (!_scale9Enabled)
    {
        return;
    }
    _buttonNormalRenderer->setCapInsets(_capInsetsNormal);
}

void Button::setCapInsetsPressedRenderer(const Rect& capInsets)
{
    _capInsetsPressed = Helper::restrictCapInsetRect(capInsets, this->_pressedTextureSize);

    // for performance issue
    if (!_scale9Enabled)
    {
        return;
    }
    _buttonClickedRenderer->setCapInsets(_capInsetsPressed);
}

void Button::setCapInsetsDisabledRenderer(const Rect& capInsets)
{
    _capInsetsDisabled = Helper::restrictCapInsetRect(capInsets, this->_disabledTextureSize);

    // for performance issue
    if (!_scale9Enabled)
    {
        return;
    }
    _buttonDisabledRenderer->setCapInsets(_capInsetsDisabled);
}

const Rect& Button::getCapInsetsNormalRenderer() const
{
    return _capInsetsNormal;
}

const Rect& Button::getCapInsetsPressedRenderer() const
{
    return _capInsetsPressed;
}

const Rect& Button::getCapInsetsDisabledRenderer() const
{
    return _capInsetsDisabled;
}

void Button::onPressStateChangedToNormal()
{
    _buttonNormalRenderer->setVisible(true);
    _buttonClickedRenderer->setVisible(false);
    _buttonDisabledRenderer->setVisible(false);
    _buttonNormalRenderer->setState(Scale9Sprite::State::NORMAL);

    if (_pressedTextureLoaded)
    {
        if (_pressedActionEnabled)
        {
            _buttonNormalRenderer->stopAllActions();
            _buttonClickedRenderer->stopAllActions();

            //            Action *zoomAction = ScaleTo::create(ZOOM_ACTION_TIME_STEP, _normalTextureScaleXInSize,
            //            _normalTextureScaleYInSize);
            // fixme: the zoomAction will run in the next frame which will cause the _buttonNormalRenderer to a wrong
            // scale
            _buttonNormalRenderer->setScale(1.0);
            _buttonClickedRenderer->setScale(1.0);

            if (nullptr != _titleRenderer)
            {
                _titleRenderer->stopAllActions();
                if (_unifySize)
                {
                    Action* zoomTitleAction = ScaleTo::create(ZOOM_ACTION_TIME_STEP, 1.0f, 1.0f);
                    _titleRenderer->runAction(zoomTitleAction);
                }
                else
                {
                    _titleRenderer->setScaleX(1.0f);
                    _titleRenderer->setScaleY(1.0f);
                }
            }
        }
    }
    else
    {
        _buttonNormalRenderer->stopAllActions();
        _buttonNormalRenderer->setScale(1.0);

        if (nullptr != _titleRenderer)
        {
            _titleRenderer->stopAllActions();
            _titleRenderer->setScaleX(1.0f);
            _titleRenderer->setScaleY(1.0f);
        }
    }
}

void Button::onPressStateChangedToPressed()
{
    _buttonNormalRenderer->setState(Scale9Sprite::State::NORMAL);

    if (_pressedTextureLoaded)
    {
        _buttonNormalRenderer->setVisible(false);
        _buttonClickedRenderer->setVisible(true);
        _buttonDisabledRenderer->setVisible(false);

        if (_pressedActionEnabled)
        {
            _buttonNormalRenderer->stopAllActions();
            _buttonClickedRenderer->stopAllActions();

            Action* zoomAction = ScaleTo::create(ZOOM_ACTION_TIME_STEP, 1.0f + _zoomScale, 1.0f + _zoomScale);
            _buttonClickedRenderer->runAction(zoomAction);

            _buttonNormalRenderer->setScale(1.0f + _zoomScale, 1.0f + _zoomScale);

            if (nullptr != _titleRenderer)
            {
                _titleRenderer->stopAllActions();
                Action* zoomTitleAction = ScaleTo::create(ZOOM_ACTION_TIME_STEP, 1.0f + _zoomScale, 1.0f + _zoomScale);
                _titleRenderer->runAction(zoomTitleAction);
            }
        }
    }
    else
    {
        _buttonNormalRenderer->setVisible(true);
        _buttonClickedRenderer->setVisible(true);
        _buttonDisabledRenderer->setVisible(false);

        _buttonNormalRenderer->stopAllActions();
        _buttonNormalRenderer->setScale(1.0f + _zoomScale, 1.0f + _zoomScale);

        if (nullptr != _titleRenderer)
        {
            _titleRenderer->stopAllActions();
            _titleRenderer->setScaleX(1.0f + _zoomScale);
            _titleRenderer->setScaleY(1.0f + _zoomScale);
        }
    }
}

void Button::onPressStateChangedToDisabled()
{
    // if disable resource is null
    if (!_disabledTextureLoaded)
    {
        if (_normalTextureLoaded)
        {
            _buttonNormalRenderer->setState(Scale9Sprite::State::GRAY);
        }
    }
    else
    {
        _buttonNormalRenderer->setVisible(false);
        _buttonDisabledRenderer->setVisible(true);
    }

    _buttonClickedRenderer->setVisible(false);
    _buttonNormalRenderer->setScale(1.0);
    _buttonClickedRenderer->setScale(1.0);
}

void Button::updateTitleLocation()
{
    _titleRenderer->setPosition(_contentSize.width * 0.5f, _contentSize.height * 0.5f);
}

void Button::updateContentSize()
{
    if (_unifySize)
    {
        if (_scale9Enabled)
        {
            ProtectedNode::setContentSize(_customSize);
        }
        else
        {
            Vec2 s = getNormalSize();
            ProtectedNode::setContentSize(s);
        }
        onSizeChanged();
        return;
    }

    if (_ignoreSize)
    {
        this->setContentSize(getVirtualRendererSize());
    }
}

void Button::onSizeChanged()
{
    Widget::onSizeChanged();
    if (nullptr != _titleRenderer)
    {
        updateTitleLocation();
    }
    _normalTextureAdaptDirty   = true;
    _pressedTextureAdaptDirty  = true;
    _disabledTextureAdaptDirty = true;
}

void Button::adaptRenderers()
{
    if (_normalTextureAdaptDirty)
    {
        normalTextureScaleChangedWithSize();
        _normalTextureAdaptDirty = false;
    }

    if (_pressedTextureAdaptDirty)
    {
        pressedTextureScaleChangedWithSize();
        _pressedTextureAdaptDirty = false;
    }

    if (_disabledTextureAdaptDirty)
    {
        disabledTextureScaleChangedWithSize();
        _disabledTextureAdaptDirty = false;
    }
}

Vec2 Button::getVirtualRendererSize() const
{
    if (_unifySize)
    {
        return this->getNormalSize();
    }

    if (nullptr != _titleRenderer)
    {
        Vec2 titleSize = _titleRenderer->getContentSize();
        if (!_normalTextureLoaded && !_titleRenderer->getString().empty())
        {
            return titleSize;
        }
    }
    return _normalTextureSize;
}

Node* Button::getVirtualRenderer()
{
    if (_bright)
    {
        switch (_brightStyle)
        {
        case BrightStyle::NORMAL:
            return _buttonNormalRenderer;
        case BrightStyle::HIGHLIGHT:
            return _buttonClickedRenderer;
        default:
            return nullptr;
        }
    }
    else
    {
        return _buttonDisabledRenderer;
    }
}

void Button::normalTextureScaleChangedWithSize()
{
    _buttonNormalRenderer->setPreferredSize(_contentSize);

    _buttonNormalRenderer->setPosition(_contentSize.width / 2.0f, _contentSize.height / 2.0f);
}

void Button::pressedTextureScaleChangedWithSize()
{
    _buttonClickedRenderer->setPreferredSize(_contentSize);

    _buttonClickedRenderer->setPosition(_contentSize.width / 2.0f, _contentSize.height / 2.0f);
}

void Button::disabledTextureScaleChangedWithSize()
{
    _buttonDisabledRenderer->setPreferredSize(_contentSize);

    _buttonDisabledRenderer->setPosition(_contentSize.width / 2.0f, _contentSize.height / 2.0f);
}

void Button::setPressedActionEnabled(bool enabled)
{
    _pressedActionEnabled = enabled;
}

void Button::setTitleAlignment(TextHAlignment hAlignment)
{
    createTitleRendererIfNull();
    _titleRenderer->setAlignment(hAlignment);
}

void Button::setTitleAlignment(TextHAlignment hAlignment, TextVAlignment vAlignment)
{
    createTitleRendererIfNull();
    _titleRenderer->setAlignment(hAlignment, vAlignment);
}

void Button::setTitleText(std::string_view text)
{
    if (text.compare(getTitleText()) == 0)
    {
        return;
    }

    createTitleRendererIfNull();

    if (getTitleFontSize() <= 0)
    {
        setTitleFontSize(CC_DEFAULT_FONT_LABEL_SIZE);
    }
    _titleRenderer->setString(text);

    updateContentSize();
    updateTitleLocation();
}

std::string_view Button::getTitleText() const
{
    if (!_titleRenderer)
    {
        return "";
    }

    return _titleRenderer->getString();
}

void Button::setTitleColor(const Color3B& color)
{
    createTitleRendererIfNull();
    _titleRenderer->setTextColor(Color4B(color));
}

Color3B Button::getTitleColor() const
{
    if (nullptr == _titleRenderer)
    {
        return Color3B::WHITE;
    }
    return Color3B(_titleRenderer->getTextColor());
}

void Button::setTitleFontSize(float size)
{
    createTitleRendererIfNull();

    Label::LabelType titleLabelType = _titleRenderer->getLabelType();
    if (titleLabelType == Label::LabelType::TTF)
    {
        TTFConfig config = _titleRenderer->getTTFConfig();
        config.fontSize  = size;
        _titleRenderer->setTTFConfig(config);
    }
    else if (titleLabelType == Label::LabelType::STRING_TEXTURE)
    {
        // the system font
        _titleRenderer->setSystemFontSize(size);
    }

    // we can't change font size of BMFont.
    if (titleLabelType != Label::LabelType::BMFONT)
    {
        updateContentSize();
    }
}

float Button::getTitleFontSize() const
{
    if (_titleRenderer)
    {
        return _titleRenderer->getRenderingFontSize();
    }

    return -1;
}

void Button::setZoomScale(float scale)
{
    _zoomScale = scale;
}

float Button::getZoomScale() const
{
    return _zoomScale;
}

void Button::setTitleFontName(std::string_view fontName)
{
    createTitleRendererIfNull();

    if (FileUtils::getInstance()->isFileExist(fontName))
    {
        std::string lowerCasedFontName{fontName};
        std::transform(lowerCasedFontName.begin(), lowerCasedFontName.end(), lowerCasedFontName.begin(), ::tolower);
        if (lowerCasedFontName.find(".fnt") != std::string::npos)
        {
            _titleRenderer->setBMFontFilePath(fontName);
        }
        else
        {
            TTFConfig config    = _titleRenderer->getTTFConfig();
            config.fontFilePath = fontName;
            _titleRenderer->setTTFConfig(config);
        }
    }
    else
    {
        _titleRenderer->setSystemFontName(fontName);
    }
    _fontName = fontName;
    updateContentSize();
}

Label* Button::getTitleRenderer() const
{
    return _titleRenderer;
}

std::string_view Button::getTitleFontName() const
{
    if (_titleRenderer)
    {
        Label::LabelType titleLabelType = _titleRenderer->getLabelType();
        if (titleLabelType == Label::LabelType::STRING_TEXTURE)
        {
            return _titleRenderer->getSystemFontName();
        }
        else if (titleLabelType == Label::LabelType::TTF)
        {
            return _titleRenderer->getTTFConfig().fontFilePath;
        }
        else if (titleLabelType == Label::LabelType::BMFONT)
        {
            return _titleRenderer->getBMFontFilePath();
        }
    }

    return ""sv;
}

std::string Button::getDescription() const
{
    return "Button";
}

Widget* Button::createCloneInstance()
{
    return Button::create();
}

void Button::copySpecialProperties(Widget* widget)
{
    Button* button = dynamic_cast<Button*>(widget);
    if (button)
    {
        _prevIgnoreSize = button->_prevIgnoreSize;
        setScale9Enabled(button->_scale9Enabled);

        // clone the inner sprite: https://github.com/cocos2d/cocos2d-x/issues/16924
        button->_buttonNormalRenderer->copyTo(_buttonNormalRenderer);
        _normalFileName      = button->_normalFileName;
        _normalTextureSize   = button->_normalTextureSize;
        _normalTexType       = button->_normalTexType;
        _normalTextureLoaded = button->_normalTextureLoaded;
        setupNormalTexture(!_normalFileName.empty());

        button->_buttonClickedRenderer->copyTo(_buttonClickedRenderer);
        _clickedFileName      = button->_clickedFileName;
        _pressedTextureSize   = button->_pressedTextureSize;
        _pressedTexType       = button->_pressedTexType;
        _pressedTextureLoaded = button->_pressedTextureLoaded;
        setupPressedTexture(!_clickedFileName.empty());

        button->_buttonDisabledRenderer->copyTo(_buttonDisabledRenderer);
        _disabledFileName      = button->_disabledFileName;
        _disabledTextureSize   = button->_disabledTextureSize;
        _disabledTexType       = button->_disabledTexType;
        _disabledTextureLoaded = button->_disabledTextureLoaded;
        setupDisabledTexture(!_disabledFileName.empty());

        setCapInsetsNormalRenderer(button->_capInsetsNormal);
        setCapInsetsPressedRenderer(button->_capInsetsPressed);
        setCapInsetsDisabledRenderer(button->_capInsetsDisabled);
        if (nullptr != button->getTitleRenderer())
        {
            setTitleText(button->getTitleText());
            setTitleFontName(button->getTitleFontName());
            setTitleFontSize(button->getTitleFontSize());
            setTitleColor(button->getTitleColor());
        }
        setPressedActionEnabled(button->_pressedActionEnabled);
        setZoomScale(button->_zoomScale);
    }
}
Vec2 Button::getNormalSize() const
{
    Vec2 titleSize;
    if (_titleRenderer != nullptr)
    {
        titleSize = _titleRenderer->getContentSize();
    }
    Vec2 imageSize;
    if (_buttonNormalRenderer != nullptr)
    {
        imageSize = _buttonNormalRenderer->getContentSize();
    }
    float width  = titleSize.width > imageSize.width ? titleSize.width : imageSize.width;
    float height = titleSize.height > imageSize.height ? titleSize.height : imageSize.height;

    return Vec2(width, height);
}

Vec2 Button::getNormalTextureSize() const
{
    return _normalTextureSize;
}

void Button::resetNormalRender()
{
    _normalFileName = "";
    _normalTexType  = TextureResType::LOCAL;

    _normalTextureSize = Vec2(0, 0);

    _normalTextureLoaded     = false;
    _normalTextureAdaptDirty = false;

    _buttonNormalRenderer->resetRender();
}
void Button::resetPressedRender()
{
    _clickedFileName = "";
    _pressedTexType  = TextureResType::LOCAL;

    _pressedTextureSize = Vec2(0, 0);

    _pressedTextureLoaded     = false;
    _pressedTextureAdaptDirty = false;

    _buttonClickedRenderer->resetRender();
}

void Button::resetDisabledRender()
{
    _disabledFileName = "";
    _disabledTexType  = TextureResType::LOCAL;

    _disabledTextureSize = Vec2(0, 0);

    _disabledTextureLoaded     = false;
    _disabledTextureAdaptDirty = false;

    _buttonDisabledRenderer->resetRender();
}

ResourceData Button::getNormalFile()
{
    ResourceData rData;
    rData.type = (int)_normalTexType;
    rData.file = _normalFileName;
    return rData;
}
ResourceData Button::getPressedFile()
{
    ResourceData rData;
    rData.type = (int)_pressedTexType;
    rData.file = _clickedFileName;
    return rData;
}
ResourceData Button::getDisabledFile()
{
    ResourceData rData;
    rData.type = (int)_disabledTexType;
    rData.file = _disabledFileName;
    return rData;
}

}  // namespace ui

NS_CC_END