#include "musictagger/TagWriter.hpp"

#include <taglib/fileref.h>
#include <taglib/tag.h>
#include <taglib/tbytevector.h>
#include <taglib/tfile.h>
#include <taglib/tpropertymap.h>
#include <taglib/tvariant.h>

namespace musictagger::tagging {
namespace {

void setProperty(TagLib::PropertyMap& properties,
                 const std::string& key,
                 const std::string& value) {
    if (value.empty()) {
        return;
    }

    TagLib::String tagKey(key.c_str(), TagLib::String::UTF8);
    TagLib::String tagValue(value.c_str(), TagLib::String::UTF8);

    properties[tagKey].clear();
    properties[tagKey].append(tagValue);
}

bool embedArtwork(TagLib::FileRef& file, const ArtworkData& artwork) {
    if (artwork.bytes.empty()) {
        return false;
    }

    TagLib::ByteVector imageData(
        reinterpret_cast<const char*>(artwork.bytes.data()),
        artwork.bytes.size()
    );

    TagLib::VariantMap picture;
    picture["data"] = imageData;
    picture["pictureType"] = TagLib::String("Front Cover");
    picture["mimeType"] = TagLib::String(artwork.contentType.c_str(), TagLib::String::UTF8);

    TagLib::List<TagLib::VariantMap> pictures;
    pictures.append(picture);

    return file.setComplexProperties("PICTURE", pictures);
}

}

std::optional<ExistingTags> readTags(const std::string& path) {
    TagLib::FileRef file(path.c_str());

    if (file.isNull() || file.tag() == nullptr) {
        return std::nullopt;
    }

    TagLib::Tag* tag = file.tag();

    return ExistingTags{
        tag->title().to8Bit(true),
        tag->artist().to8Bit(true),
        tag->album().to8Bit(true),
        tag->year(),
        tag->track()
    };
}

bool writeTags(const std::string& path,
               const RecordingMatch& recording,
               const ReleaseCandidate& release,
               const TaggingDetails& details,
               const ArtworkData* artwork) {
    TagLib::FileRef file(path.c_str());

    if (file.isNull() || file.file() == nullptr || file.file()->readOnly()) {
        return false;
    }

    TagLib::PropertyMap properties = file.properties();

    setProperty(properties, "TITLE", recording.title);
    setProperty(properties, "ARTIST", recording.artist);
    setProperty(properties, "ALBUM", release.title);
    setProperty(properties, "ALBUMARTIST", details.albumArtist);
    setProperty(properties, "DATE", release.date);
    setProperty(properties, "RELEASECOUNTRY", release.country);
    setProperty(properties, "RELEASETYPE", release.type);
    setProperty(properties, "MUSICBRAINZ_TRACKID", recording.id);
    setProperty(properties, "MUSICBRAINZ_ALBUMID", release.releaseId);
    setProperty(properties, "MUSICBRAINZ_RELEASETRACKID", details.releaseTrackId);

    if (details.trackNumber > 0) {
        std::string track = std::to_string(details.trackNumber);
        if (details.trackTotal > 0) {
            track += "/" + std::to_string(details.trackTotal);
        }
        setProperty(properties, "TRACKNUMBER", track);
    }

    if (details.discNumber > 0) {
        std::string disc = std::to_string(details.discNumber);
        if (details.discTotal > 1) {
            disc += "/" + std::to_string(details.discTotal);
        }
        setProperty(properties, "DISCNUMBER", disc);
    }

    file.setProperties(properties);

    if (artwork != nullptr && !artwork->bytes.empty()) {
        embedArtwork(file, *artwork);
    }

    return file.save();
}

}
