// SPDX-License-Identifier: GPL-3.0-or-later

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QTimer>

#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "btreeidx.hh"
#include "langcoder.hh"
#include "wordfinder.hh"
#include "wstring_qt.hh"

namespace Qt5AcceptanceObserver {
namespace {

int const kIndexingTimeoutMilliseconds = 5 * 60 * 1000;

QString FormatForComponents( std::vector< std::string > const & components )
{
  for( std::vector< std::string >::const_iterator component = components.begin();
       component != components.end(); ++component )
  {
    QString const path = QString::fromUtf8( component->c_str() ).toLower();
    if( path.endsWith( ".dsl" ) || path.endsWith( ".dsl.dz" ) )
      return "dsl";
    if( path.endsWith( ".mdx" ) )
      return "mdict";
    if( path.endsWith( ".ifo" ) )
      return "stardict";
    if( path.endsWith( ".index" ) )
      return "dictd";
    if( path.endsWith( ".dct" ) )
      return "sdict";
    if( path.endsWith( ".xdxf" ) || path.endsWith( ".xdxf.dz" ) )
      return "xdxf";
    if( path.endsWith( ".gls" ) || path.endsWith( ".gls.dz" ) )
      return "gls";
    if( path.endsWith( ".bgl" ) )
      return "bgl";
    if( path.endsWith( ".aar" ) )
      return "aard";
    if( path.endsWith( ".zim" ) || path.endsWith( ".zimaa" ) )
      return "zim";
    if( path.endsWith( ".slob" ) )
      return "slob";
    if( path.endsWith( ".lsa" ) )
      return "lsa";
    if( path.endsWith( ".zip" ) )
      return "zipsounds";
    if( path.endsWith( "/honmon" ) || path.endsWith( "\\honmon" ) )
      return "epwing";
  }
  return QString();
}

QJsonObject CatalogEntry( Dictionary::Class & dictionary, int order )
{
  QJsonArray components;
  std::vector< std::string > const & files = dictionary.getDictionaryFilenames();
  for( std::vector< std::string >::const_iterator file = files.begin();
       file != files.end(); ++file )
    components.append( QString::fromUtf8( file->c_str() ) );

  QJsonObject entry;
  entry.insert( "article_count", static_cast< qint64 >( dictionary.getArticleCount() ) );
  entry.insert( "components", components );
  entry.insert( "edition", QString() );
  entry.insert( "enabled", true );
  entry.insert( "format", FormatForComponents( files ) );
  entry.insert( "headword_count", static_cast< qint64 >( dictionary.getWordCount() ) );
  entry.insert( "id", QString::fromUtf8( dictionary.getId().c_str() ) );
  entry.insert( "name", QString::fromUtf8( dictionary.getName().c_str() ) );
  entry.insert( "order", order );
  entry.insert( "source_language", LangCoder::intToCode2( dictionary.getLangFrom() ) );
  entry.insert( "target_language", LangCoder::intToCode2( dictionary.getLangTo() ) );
  return entry;
}

void ScheduleExit()
{
  QTimer::singleShot( 0, qApp, SLOT( quit() ) );
}

QJsonArray Catalog( std::vector< sptr< Dictionary::Class > > const & dictionaries )
{
  QJsonArray catalog;
  int order = 0;
  for( std::vector< sptr< Dictionary::Class > >::const_iterator dictionary = dictionaries.begin();
       dictionary != dictionaries.end(); ++dictionary )
  {
    if( ( *dictionary )->getDictionaryFilenames().empty() )
      continue;
    catalog.append( CatalogEntry( **dictionary, order++ ) );
  }
  return catalog;
}

QJsonObject Phase( QByteArray const & name, QByteArray const & status,
                   int sequence, std::string const & dictionaryId = std::string() )
{
  QJsonObject phase;
  phase.insert( "dictionary_id", QString::fromUtf8( dictionaryId.c_str() ) );
  phase.insert( "name", QString::fromLatin1( name ) );
  phase.insert( "sequence", sequence );
  phase.insert( "status", QString::fromLatin1( status ) );
  return phase;
}

bool WriteRaw( QByteArray const & outputPath, QByteArray const & conditionsHash,
               QByteArray const & scenario, QJsonArray const & catalog,
               QJsonArray const & phases, QByteArray const & outcome )
{
  QJsonObject raw;
  raw.insert( "catalog", catalog );
  raw.insert( "conditions_sha256", QString::fromLatin1( conditionsHash ) );
  raw.insert( "elapsed_milliseconds", 0 );
  raw.insert( "errors", QJsonArray() );
  raw.insert( "outcome", QString::fromLatin1( outcome ) );
  raw.insert( "phases", phases );
  raw.insert( "scenario", QString::fromLatin1( scenario ) );
  raw.insert( "schema", "goldendict-real-dictionary-raw-observation-v1" );

  QSaveFile output( QString::fromUtf8( outputPath ) );
  if( !output.open( QIODevice::WriteOnly ) )
    return false;
  QByteArray const content = QJsonDocument( raw ).toJson( QJsonDocument::Compact ) + '\n';
  return output.write( content ) == content.size() && output.commit();
}

Dictionary::Class * FindIndexingDictionary(
  std::vector< sptr< Dictionary::Class > > const & dictionaries,
  QString const & name )
{
  for( std::vector< sptr< Dictionary::Class > >::const_iterator dictionary = dictionaries.begin();
       dictionary != dictionaries.end(); ++dictionary )
    if( QString::fromUtf8( ( *dictionary )->getName().c_str() ) == name )
      return dictionary->get();
  return 0;
}

QString MediaTypeForResource( QString const & resourceId )
{
  QString const lower = resourceId.toLower();
  if( lower.endsWith( ".png" ) )
    return "image/png";
  if( lower.endsWith( ".jpg" ) || lower.endsWith( ".jpeg" ) )
    return "image/jpeg";
  if( lower.endsWith( ".mp3" ) )
    return "audio/mpeg";
  if( lower.endsWith( ".wav" ) )
    return "audio/wav";
  return "application/octet-stream";
}

class MdictPublication: public QObject
{
  QJsonObject catalog;
  QByteArray catalogHash;
  QByteArray conditionsHash;
  QByteArray outputPath;
  QByteArray scenario;
  Dictionary::Class * dictionary;
  QJsonArray probes;
  QJsonArray results;
  int probeIndex;
  bool readingResource;
  QElapsedTimer elapsed;
  sptr< Dictionary::DataRequest > request;
  QByteArray articleMarkup;

public:
  MdictPublication( QObject * owner,
                    std::vector< sptr< Dictionary::Class > > const & dictionaries,
                    QByteArray const & catalogPath,
                    QByteArray const & catalogHash_,
                    QByteArray const & conditionsHash_,
                    QByteArray const & outputPath_,
                    QByteArray const & scenario_ ):
    QObject( owner ), catalogHash( catalogHash_ ), conditionsHash( conditionsHash_ ),
    outputPath( outputPath_ ), scenario( scenario_ ), dictionary( 0 ),
    probeIndex( 0 ), readingResource( false )
  {
    QFile input( QString::fromUtf8( catalogPath ) );
    if( !input.open( QIODevice::ReadOnly ) || input.size() < 1
        || input.size() > 1024 * 1024 )
      return;
    QByteArray const content = input.readAll();
    if( content.size() != input.size()
        || QCryptographicHash::hash( content, QCryptographicHash::Sha256 ).toHex()
             != catalogHash )
      return;
    QJsonParseError error;
    QJsonDocument const document = QJsonDocument::fromJson( content, &error );
    if( error.error != QJsonParseError::NoError || !document.isObject() )
      return;
    catalog = document.object();
    if( catalog.value( "schema" ).toString()
          != "goldendict-mdict-query-resource-catalog-v1" )
      return;
    probes = catalog.value( "probes" ).toArray();
    if( probes.size() != 3 )
      return;
    QString const primaryRelative = catalog.value( "dictionary" ).toObject()
                                      .value( "primary_component" ).toString();
    QByteArray const corpusRoot = qgetenv( "GOLDENDICT_ACCEPTANCE_CORPUS_ROOT" );
    QString const primary = QFileInfo( QDir( QString::fromUtf8( corpusRoot ) )
                                      .filePath( primaryRelative ) ).canonicalFilePath();
    for( std::vector< sptr< Dictionary::Class > >::const_iterator item = dictionaries.begin();
         item != dictionaries.end(); ++item )
    {
      std::vector< std::string > const & files = ( *item )->getDictionaryFilenames();
      if( !files.empty()
          && QFileInfo( QString::fromUtf8( files.front().c_str() ) ).canonicalFilePath()
               .compare( primary, Qt::CaseInsensitive ) == 0 )
      {
        dictionary = item->get();
        break;
      }
    }
  }

  void Start()
  {
    if( !dictionary )
    {
      qApp->exit( 4 );
      return;
    }
    elapsed.start();
    StartArticle();
  }

private:
  void StartArticle()
  {
    if( probeIndex == probes.size() )
    {
      Publish();
      return;
    }
    QJsonObject const probe = probes.at( probeIndex ).toObject();
    QString const query = probe.value( "query" ).toObject().value( "text" ).toString();
    if( query.isEmpty() )
    {
      Fail();
      return;
    }
    try
    {
      request = dictionary->getArticle( gd::toWString( query ),
                                        std::vector< gd::wstring >() );
    }
    catch( std::exception const & )
    {
      Fail();
      return;
    }
    readingResource = false;
    Poll();
  }

  void StartResource()
  {
    QJsonObject const probe = probes.at( probeIndex ).toObject();
    QString const resourceId = probe.value( "resource" ).toObject().value( "id" ).toString();
    try
    {
      request = dictionary->getResource( std::string( resourceId.toUtf8().constData() ) );
    }
    catch( std::exception const & )
    {
      Fail();
      return;
    }
    readingResource = true;
    Poll();
  }

  void Poll()
  {
    if( !request || elapsed.elapsed() > 5 * 60 * 1000 )
    {
      Fail();
      return;
    }
    if( !request->isFinished() )
    {
      QTimer::singleShot( 1, this, [this]() { Poll(); } );
      return;
    }
    if( !request->getErrorString().isEmpty() || request->dataSize() < 1 )
    {
      Fail();
      return;
    }
    std::vector< char > & data = request->getFullData();
    QByteArray const bytes( &data.front(), static_cast< int >( data.size() ) );
    request = sptr< Dictionary::DataRequest >();
    if( !readingResource )
    {
      articleMarkup = bytes;
      StartResource();
      return;
    }

    QJsonObject const probe = probes.at( probeIndex ).toObject();
    QString const id = probe.value( "id" ).toString();
    QString const query = probe.value( "query" ).toObject().value( "text" ).toString();
    QString const resourceId = probe.value( "resource" ).toObject().value( "id" ).toString();
    QJsonObject result;
    result.insert( "article_markup", QString::fromUtf8( articleMarkup ) );
    result.insert( "headword", query );
    result.insert( "id", id );
    result.insert( "query", query );
    result.insert( "resource_data_base64", QString::fromLatin1( bytes.toBase64() ) );
    result.insert( "resource_id", resourceId );
    result.insert( "resource_media_type", MediaTypeForResource( resourceId ) );
    results.append( result );
    ++probeIndex;
    StartArticle();
  }

  void Publish()
  {
    QJsonArray components;
    std::vector< std::string > const & files = dictionary->getDictionaryFilenames();
    for( std::vector< std::string >::const_iterator file = files.begin();
         file != files.end(); ++file )
      components.append( QFileInfo( QString::fromUtf8( file->c_str() ) ).canonicalFilePath() );
    QJsonObject dictionaryValue;
    dictionaryValue.insert( "components", components );
    dictionaryValue.insert( "id", QString::fromUtf8( dictionary->getId().c_str() ) );
    dictionaryValue.insert( "name", QString::fromUtf8( dictionary->getName().c_str() ) );
    QJsonObject raw;
    raw.insert( "catalog_sha256", QString::fromLatin1( catalogHash ) );
    raw.insert( "conditions_sha256", QString::fromLatin1( conditionsHash ) );
    raw.insert( "dictionary", dictionaryValue );
    raw.insert( "errors", QJsonArray() );
    raw.insert( "probes", results );
    raw.insert( "scenario", QString::fromLatin1( scenario ) );
    raw.insert( "schema", "goldendict-real-mdict-raw-observation-v1" );
    if( !WriteRawMdict( raw ) )
    {
      Fail();
      return;
    }
    ScheduleExit();
    deleteLater();
  }

  bool WriteRawMdict( QJsonObject const & raw )
  {
    QSaveFile output( QString::fromUtf8( outputPath ) );
    if( !output.open( QIODevice::WriteOnly ) )
      return false;
    QByteArray const content = QJsonDocument( raw ).toJson( QJsonDocument::Compact ) + '\n';
    return output.write( content ) == content.size() && output.commit();
  }

  void Fail()
  {
    request = sptr< Dictionary::DataRequest >();
    qApp->exit( 4 );
    deleteLater();
  }
};

class DslPublication: public QObject
{
  QJsonObject catalog;
  QByteArray catalogHash;
  QByteArray conditionsHash;
  QByteArray outputPath;
  QByteArray scenario;
  std::vector< sptr< Dictionary::Class > > const & dictionaries;
  QJsonArray catalogDictionaries;
  QJsonArray results;
  int dictionaryIndex;
  bool readingResource;
  QElapsedTimer elapsed;
  sptr< Dictionary::DataRequest > request;
  QByteArray articleMarkup;
  Dictionary::Class * dictionary;

public:
  DslPublication( QObject * owner,
                  std::vector< sptr< Dictionary::Class > > const & dictionaries_,
                  QByteArray const & catalogPath,
                  QByteArray const & catalogHash_,
                  QByteArray const & conditionsHash_,
                  QByteArray const & outputPath_,
                  QByteArray const & scenario_ ):
    QObject( owner ), catalogHash( catalogHash_ ), conditionsHash( conditionsHash_ ),
    outputPath( outputPath_ ), scenario( scenario_ ), dictionaries( dictionaries_ ),
    dictionaryIndex( 0 ), readingResource( false ), dictionary( 0 )
  {
    QFile input( QString::fromUtf8( catalogPath ) );
    if( !input.open( QIODevice::ReadOnly ) || input.size() < 1
        || input.size() > 1024 * 1024 )
      return;
    QByteArray const content = input.readAll();
    if( content.size() != input.size()
        || QCryptographicHash::hash( content, QCryptographicHash::Sha256 ).toHex()
             != catalogHash )
      return;
    QJsonParseError error;
    QJsonDocument const document = QJsonDocument::fromJson( content, &error );
    if( error.error != QJsonParseError::NoError || !document.isObject() )
      return;
    catalog = document.object();
    if( catalog.value( "schema" ).toString()
          != "goldendict-dsl-query-resource-catalog-v1" )
      return;
    catalogDictionaries = catalog.value( "dictionaries" ).toArray();
    if( catalogDictionaries.size() != 5 )
      return;
  }

  void Start()
  {
    if( catalog.isEmpty() )
    {
      Fail();
      return;
    }
    elapsed.start();
    StartDictionary();
  }

private:
  QString CanonicalCatalogPath( QString const & relative ) const
  {
    QByteArray const corpusRoot = qgetenv( "GOLDENDICT_ACCEPTANCE_CORPUS_ROOT" );
    return QFileInfo( QDir( QString::fromUtf8( corpusRoot ) ).filePath( relative ) )
      .canonicalFilePath();
  }

  Dictionary::Class * FindDictionary( QString const & primary ) const
  {
    QString const canonicalPrimary = CanonicalCatalogPath( primary );
    for( std::vector< sptr< Dictionary::Class > >::const_iterator item = dictionaries.begin();
         item != dictionaries.end(); ++item )
    {
      std::vector< std::string > const & files = ( *item )->getDictionaryFilenames();
      if( !files.empty()
          && QFileInfo( QString::fromUtf8( files.front().c_str() ) ).canonicalFilePath()
               .compare( canonicalPrimary, Qt::CaseInsensitive ) == 0 )
        return item->get();
    }
    return 0;
  }

  void StartDictionary()
  {
    if( dictionaryIndex == catalogDictionaries.size() )
    {
      Publish();
      return;
    }
    QJsonObject const entry = catalogDictionaries.at( dictionaryIndex ).toObject();
    dictionary = FindDictionary( entry.value( "primary_component" ).toString() );
    if( !dictionary )
    {
      Fail();
      return;
    }
    QString const query = entry.value( "probe" ).toObject()
                            .value( "query" ).toObject().value( "text" ).toString();
    if( query.isEmpty() )
    {
      Fail();
      return;
    }
    try
    {
      request = dictionary->getArticle( gd::toWString( query ),
                                        std::vector< gd::wstring >() );
    }
    catch( std::exception const & )
    {
      Fail();
      return;
    }
    readingResource = false;
    Poll();
  }

  void StartResource()
  {
    QJsonObject const entry = catalogDictionaries.at( dictionaryIndex ).toObject();
    QString const resourceId = entry.value( "probe" ).toObject()
                                 .value( "resource" ).toObject().value( "id" ).toString();
    try
    {
      request = dictionary->getResource( std::string( resourceId.toUtf8().constData() ) );
    }
    catch( std::exception const & )
    {
      CompleteProbe( QByteArray(), false );
      return;
    }
    readingResource = true;
    Poll();
  }

  void Poll()
  {
    if( !request || elapsed.elapsed() > 15 * 60 * 1000 )
    {
      Fail();
      return;
    }
    if( !request->isFinished() )
    {
      QTimer::singleShot( 1, this, [this]() { Poll(); } );
      return;
    }
    bool const available = request->getErrorString().isEmpty()
                           && request->dataSize() > 0;
    QByteArray bytes;
    if( available )
    {
      std::vector< char > & data = request->getFullData();
      bytes = QByteArray( &data.front(), static_cast< int >( data.size() ) );
    }
    request = sptr< Dictionary::DataRequest >();
    if( !readingResource )
    {
      if( !available )
      {
        Fail();
        return;
      }
      articleMarkup = bytes;
      StartResource();
      return;
    }
    CompleteProbe( bytes, available );
  }

  void CompleteProbe( QByteArray const & resourceBytes, bool available )
  {
    QJsonObject const entry = catalogDictionaries.at( dictionaryIndex ).toObject();
    QJsonObject const probe = entry.value( "probe" ).toObject();
    QString const query = probe.value( "query" ).toObject().value( "text" ).toString();
    QString const resourceId = probe.value( "resource" ).toObject().value( "id" ).toString();
    QString const archive = CanonicalCatalogPath(
      entry.value( "archive" ).toObject().value( "component" ).toString() );
    QJsonArray components;
    bool archiveOwned = false;
    std::vector< std::string > const & files = dictionary->getDictionaryFilenames();
    for( std::vector< std::string >::const_iterator file = files.begin();
         file != files.end(); ++file )
    {
      QString const canonical = QFileInfo( QString::fromUtf8( file->c_str() ) )
                                  .canonicalFilePath();
      components.append( canonical );
      archiveOwned = archiveOwned
                     || canonical.compare( archive, Qt::CaseInsensitive ) == 0;
    }
    QJsonObject rawProbe;
    rawProbe.insert( "article_markup", QString::fromUtf8( articleMarkup ) );
    rawProbe.insert( "headword", query );
    rawProbe.insert( "id", probe.value( "id" ) );
    rawProbe.insert( "query", query );
    rawProbe.insert( "resource_available", available );
    rawProbe.insert( "resource_data_base64",
                     available ? QString::fromLatin1( resourceBytes.toBase64() )
                               : QString() );
    rawProbe.insert( "resource_id", resourceId );
    rawProbe.insert( "resource_media_type", MediaTypeForResource( resourceId ) );
    QJsonObject result;
    result.insert( "archive_owned", archiveOwned );
    result.insert( "components", components );
    result.insert( "id", QString::fromUtf8( dictionary->getId().c_str() ) );
    result.insert( "name", QString::fromUtf8( dictionary->getName().c_str() ) );
    result.insert( "probe", rawProbe );
    results.append( result );
    ++dictionaryIndex;
    StartDictionary();
  }

  bool OrphanOwned() const
  {
    QJsonArray const orphans = catalog.value( "orphan_archives" ).toArray();
    if( orphans.size() != 1 )
      return true;
    QString const orphan = CanonicalCatalogPath(
      orphans.at( 0 ).toObject().value( "component" ).toString() );
    for( std::vector< sptr< Dictionary::Class > >::const_iterator item = dictionaries.begin();
         item != dictionaries.end(); ++item )
    {
      std::vector< std::string > const & files = ( *item )->getDictionaryFilenames();
      for( std::vector< std::string >::const_iterator file = files.begin();
           file != files.end(); ++file )
        if( QFileInfo( QString::fromUtf8( file->c_str() ) ).canonicalFilePath()
              .compare( orphan, Qt::CaseInsensitive ) == 0 )
          return true;
    }
    return false;
  }

  void Publish()
  {
    QJsonObject raw;
    raw.insert( "catalog_sha256", QString::fromLatin1( catalogHash ) );
    raw.insert( "conditions_sha256", QString::fromLatin1( conditionsHash ) );
    raw.insert( "dictionaries", results );
    raw.insert( "errors", QJsonArray() );
    raw.insert( "orphan_archive_owned", OrphanOwned() );
    raw.insert( "scenario", QString::fromLatin1( scenario ) );
    raw.insert( "schema", "goldendict-real-dsl-raw-observation-v1" );
    QSaveFile output( QString::fromUtf8( outputPath ) );
    if( !output.open( QIODevice::WriteOnly ) )
    {
      Fail();
      return;
    }
    QByteArray const content = QJsonDocument( raw ).toJson( QJsonDocument::Compact ) + '\n';
    if( output.write( content ) != content.size() || !output.commit() )
    {
      Fail();
      return;
    }
    ScheduleExit();
    deleteLater();
  }

  void Fail()
  {
    request = sptr< Dictionary::DataRequest >();
    qApp->exit( 4 );
    deleteLater();
  }
};

class LookupPublication: public QObject
{
  QJsonObject catalog;
  QByteArray catalogHash;
  QByteArray conditionsHash;
  QByteArray outputPath;
  QByteArray scenario;
  std::vector< sptr< Dictionary::Class > > const & dictionaries;
  QJsonArray catalogDictionaries;
  QJsonArray results;
  int dictionaryIndex;
  int probeIndex;
  Dictionary::Class * dictionary;
  sptr< Dictionary::DataRequest > articleRequest;
  sptr< Dictionary::WordSearchRequest > morphologyRequest;
  std::unique_ptr< WordFinder > wordFinder;
  std::vector< sptr< Dictionary::Class > > suggestionDictionaries;
  bool wordFinderFinished;
  QElapsedTimer elapsed;

public:
  LookupPublication( QObject * owner,
                     std::vector< sptr< Dictionary::Class > > const & dictionaries_,
                     QByteArray const & catalogPath,
                     QByteArray const & catalogHash_,
                     QByteArray const & conditionsHash_,
                     QByteArray const & outputPath_,
                     QByteArray const & scenario_ ):
    QObject( owner ), catalogHash( catalogHash_ ), conditionsHash( conditionsHash_ ),
    outputPath( outputPath_ ), scenario( scenario_ ), dictionaries( dictionaries_ ),
    dictionaryIndex( 0 ), probeIndex( 0 ), dictionary( 0 ),
    wordFinderFinished( false )
  {
    QFile input( QString::fromUtf8( catalogPath ) );
    if( !input.open( QIODevice::ReadOnly ) || input.size() < 1
        || input.size() > 1024 * 1024 )
      return;
    QByteArray const content = input.readAll();
    if( content.size() != input.size()
        || QCryptographicHash::hash( content, QCryptographicHash::Sha256 ).toHex()
             != catalogHash )
      return;
    QJsonParseError error;
    QJsonDocument const document = QJsonDocument::fromJson( content, &error );
    if( error.error != QJsonParseError::NoError || !document.isObject() )
      return;
    catalog = document.object();
    if( catalog.value( "schema" ).toString()
          != "goldendict-real-lookup-catalog-v1" )
      return;
    catalogDictionaries = catalog.value( "dictionaries" ).toArray();
    if( catalogDictionaries.isEmpty() || catalogDictionaries.size() > 32 )
      catalog = QJsonObject();
  }

  void Start()
  {
    if( catalog.isEmpty() )
    {
      Fail();
      return;
    }
    elapsed.start();
    StartProbe();
  }

private:
  Dictionary::Class * FindDictionary( QString const & relative ) const
  {
    QByteArray const corpusRoot = qgetenv( "GOLDENDICT_ACCEPTANCE_CORPUS_ROOT" );
    QString const expected = QFileInfo( QDir( QString::fromUtf8( corpusRoot ) )
                                        .filePath( relative ) ).canonicalFilePath();
    if( expected.isEmpty() )
      return 0;
    for( std::vector< sptr< Dictionary::Class > >::const_iterator item = dictionaries.begin();
         item != dictionaries.end(); ++item )
    {
      std::vector< std::string > const & files = ( *item )->getDictionaryFilenames();
      if( !files.empty()
          && QFileInfo( QString::fromUtf8( files.front().c_str() ) ).canonicalFilePath()
               .compare( expected, Qt::CaseInsensitive ) == 0 )
        return item->get();
    }
    return 0;
  }

  void StartProbe()
  {
    if( dictionaryIndex == catalogDictionaries.size() )
    {
      Publish();
      return;
    }
    QJsonObject const item = catalogDictionaries.at( dictionaryIndex ).toObject();
    QJsonArray const probes = item.value( "probes" ).toArray();
    if( !dictionary )
      dictionary = FindDictionary( item.value( "primary_component" ).toString() );
    if( !dictionary || probes.isEmpty() || probes.size() > 16 )
    {
      Fail();
      return;
    }
    if( probeIndex == probes.size() )
    {
      QJsonArray components;
      std::vector< std::string > const & files = dictionary->getDictionaryFilenames();
      if( files.empty() )
      {
        Fail();
        return;
      }
      if( item.value( "format" ).toString() == "hunspell" )
      {
        for( std::vector< std::string >::const_iterator file = files.begin();
             file != files.end(); ++file )
          components.append( QFileInfo( QString::fromUtf8( file->c_str() ) )
                               .canonicalFilePath() );
      }
      else
        components.append( QFileInfo( QString::fromUtf8( files.front().c_str() ) )
                             .canonicalFilePath() );
      QJsonObject result;
      result.insert( "catalog_id", item.value( "id" ) );
      result.insert( "components", components );
      result.insert( "id", QString::fromUtf8( dictionary->getId().c_str() ) );
      result.insert( "name", QString::fromUtf8( dictionary->getName().c_str() ) );
      result.insert( "probes", itemResults );
      results.append( result );
      itemResults = QJsonArray();
      ++dictionaryIndex;
      probeIndex = 0;
      dictionary = 0;
      StartProbe();
      return;
    }
    QJsonObject const probe = probes.at( probeIndex ).toObject();
    QString const operation = probe.value( "operation" ).toString();
    QString const query = probe.value( "query" ).toString();
    int const limit = probe.value( "result_limit" ).toInt();
    if( query.isEmpty() || limit < 1 || limit > 32 )
    {
      Fail();
      return;
    }
    try
    {
      if( operation == "lookup" )
        articleRequest = dictionary->getArticle( gd::toWString( query ),
                                                  std::vector< gd::wstring >() );
      else if( operation == "morphology" )
        morphologyRequest = dictionary->findHeadwordsForSynonym(
          gd::toWString( query ) );
      else if( operation == "suggest" )
      {
        suggestionDictionaries.clear();
        for( std::vector< sptr< Dictionary::Class > >::const_iterator item = dictionaries.begin();
             item != dictionaries.end(); ++item )
          if( item->get() == dictionary )
            suggestionDictionaries.push_back( *item );
        if( suggestionDictionaries.size() != 1 )
        {
          Fail();
          return;
        }
        wordFinder.reset( new WordFinder( 0 ) );
        wordFinderFinished = false;
        connect( wordFinder.get(), &WordFinder::finished, this,
                 [this]() { wordFinderFinished = true; } );
        wordFinder->prefixMatch( query, suggestionDictionaries, limit );
      }
      else
      {
        Fail();
        return;
      }
    }
    catch( std::exception const & )
    {
      Fail();
      return;
    }
    Poll();
  }

  QJsonArray itemResults;

  void Poll()
  {
    if( elapsed.elapsed() > 5 * 60 * 1000 )
    {
      Fail();
      return;
    }
    if( articleRequest && !articleRequest->isFinished() )
    {
      QTimer::singleShot( 1, this, [this]() { Poll(); } );
      return;
    }
    if( morphologyRequest && !morphologyRequest->isFinished() )
    {
      QTimer::singleShot( 1, this, [this]() { Poll(); } );
      return;
    }
    if( wordFinder && !wordFinderFinished )
    {
      QTimer::singleShot( 1, this, [this]() { Poll(); } );
      return;
    }
    CompleteProbe();
  }

  void CompleteProbe()
  {
    QJsonObject const item = catalogDictionaries.at( dictionaryIndex ).toObject();
    QJsonObject const probe = item.value( "probes" ).toArray().at( probeIndex ).toObject();
    QString const operation = probe.value( "operation" ).toString();
    QString const query = probe.value( "query" ).toString();
    QJsonArray entries;
    QJsonArray suggestions;
    QJsonArray errors;
    if( operation == "lookup" )
    {
      QString const error = articleRequest->getErrorString();
      if( !error.isEmpty() )
      {
        QJsonObject value;
        value.insert( "dictionary_id", QString::fromUtf8( dictionary->getId().c_str() ) );
        value.insert( "message", error );
        errors.append( value );
      }
      else if( articleRequest->dataSize() > 0 )
      {
        BtreeIndexing::BtreeDictionary * indexedDictionary =
          dynamic_cast< BtreeIndexing::BtreeDictionary * >( dictionary );
        if( !indexedDictionary )
        {
          Fail();
          return;
        }
        std::vector< BtreeIndexing::WordArticleLink > const links =
          indexedDictionary->findArticles( gd::toWString( query ) );
        if( links.empty() )
        {
          Fail();
          return;
        }
        std::vector< char > & data = articleRequest->getFullData();
        QByteArray const bytes( &data.front(), static_cast< int >( data.size() ) );
        QJsonObject entry;
        entry.insert( "article_markup", QString::fromUtf8( bytes ) );
        entry.insert( "headword", QString::fromUtf8( links.front().word.c_str() ) );
        entry.insert( "plain_text", QString() );
        entry.insert( "resources", QJsonArray() );
        entries.append( entry );
      }
      articleRequest = sptr< Dictionary::DataRequest >();
    }
    else if( operation == "morphology" )
    {
      QString const error = morphologyRequest->getErrorString();
      if( !error.isEmpty() )
      {
        QJsonObject value;
        value.insert( "dictionary_id", QString::fromUtf8( dictionary->getId().c_str() ) );
        value.insert( "message", error );
        errors.append( value );
      }
      else
      {
        std::vector< Dictionary::WordMatch > & matches =
          morphologyRequest->getAllMatches();
        int const limit = probe.value( "result_limit" ).toInt();
        for( std::size_t index = 0; index < matches.size()
             && index < static_cast< std::size_t >( limit ); ++index )
          suggestions.append( gd::toQString( matches[ index ].word ) );
      }
      morphologyRequest = sptr< Dictionary::WordSearchRequest >();
    }
    else
    {
      QString const error = wordFinder->getErrorString();
      if( !error.isEmpty() )
      {
        QJsonObject value;
        value.insert( "dictionary_id", QString::fromUtf8( dictionary->getId().c_str() ) );
        value.insert( "message", error );
        errors.append( value );
      }
      else
      {
        WordFinder::SearchResults const & matches = wordFinder->getResults();
        int const limit = probe.value( "result_limit" ).toInt();
        for( std::size_t index = 0; index < matches.size()
             && index < static_cast< std::size_t >( limit ); ++index )
          suggestions.append( matches[ index ].first );
      }
      wordFinder.reset();
      suggestionDictionaries.clear();
    }
    QJsonObject rawProbe;
    rawProbe.insert( "entries", entries );
    rawProbe.insert( "errors", errors );
    rawProbe.insert( "id", probe.value( "id" ) );
    rawProbe.insert( "operation", operation );
    rawProbe.insert( "suggestions", suggestions );
    itemResults.append( rawProbe );
    ++probeIndex;
    StartProbe();
  }

  void Publish()
  {
    QJsonObject raw;
    raw.insert( "catalog_sha256", QString::fromLatin1( catalogHash ) );
    raw.insert( "conditions_sha256", QString::fromLatin1( conditionsHash ) );
    raw.insert( "dictionaries", results );
    raw.insert( "errors", QJsonArray() );
    raw.insert( "scenario", QString::fromLatin1( scenario ) );
    raw.insert( "schema", "goldendict-real-lookup-raw-observation-v1" );
    QSaveFile output( QString::fromUtf8( outputPath ) );
    if( !output.open( QIODevice::WriteOnly ) )
    {
      Fail();
      return;
    }
    QByteArray const content = QJsonDocument( raw ).toJson( QJsonDocument::Compact ) + '\n';
    if( output.write( content ) != content.size() || !output.commit() )
    {
      Fail();
      return;
    }
    ScheduleExit();
    deleteLater();
  }

  void Fail()
  {
    articleRequest = sptr< Dictionary::DataRequest >();
    morphologyRequest = sptr< Dictionary::WordSearchRequest >();
    wordFinder.reset();
    suggestionDictionaries.clear();
    qApp->exit( 4 );
    deleteLater();
  }
};

void PublishMdict( QObject * owner,
                   std::vector< sptr< Dictionary::Class > > const & dictionaries,
                   QByteArray const & catalogPath,
                   QByteArray const & catalogHash,
                   QByteArray const & conditionsHash,
                   QByteArray const & outputPath,
                   QByteArray const & scenario )
{
  MdictPublication * publication = new MdictPublication(
    owner, dictionaries, catalogPath, catalogHash, conditionsHash, outputPath, scenario );
  QTimer::singleShot( 0, publication, [publication]() { publication->Start(); } );
}

void PublishDsl( QObject * owner,
                 std::vector< sptr< Dictionary::Class > > const & dictionaries,
                 QByteArray const & catalogPath,
                 QByteArray const & catalogHash,
                 QByteArray const & conditionsHash,
                 QByteArray const & outputPath,
                 QByteArray const & scenario )
{
  DslPublication * publication = new DslPublication(
    owner, dictionaries, catalogPath, catalogHash, conditionsHash, outputPath, scenario );
  QTimer::singleShot( 0, publication, [publication]() { publication->Start(); } );
}

void PublishLookup( QObject * owner,
                    std::vector< sptr< Dictionary::Class > > const & dictionaries,
                    QByteArray const & catalogPath,
                    QByteArray const & catalogHash,
                    QByteArray const & conditionsHash,
                    QByteArray const & outputPath,
                    QByteArray const & scenario )
{
  LookupPublication * publication = new LookupPublication(
    owner, dictionaries, catalogPath, catalogHash, conditionsHash, outputPath, scenario );
  QTimer::singleShot( 0, publication, [publication]() { publication->Start(); } );
}

class ManagementPublication: public QObject
{
public:
  ManagementPublication( QObject * parent,
                         std::vector< sptr< Dictionary::Class > > const & dictionaries_,
                         Config::Class & configuration_,
                         QByteArray const & catalogPath,
                         QByteArray const & catalogHash_,
                         QByteArray const & conditionsHash_,
                         QByteArray const & outputPath_,
                         QByteArray const & scenario_ ):
    QObject( parent ), dictionaries( dictionaries_ ), configuration( configuration_ ),
    catalogHash( catalogHash_ ), conditionsHash( conditionsHash_ ),
    outputPath( outputPath_ ), scenario( scenario_ )
  {
    QFile file( QString::fromUtf8( catalogPath ) );
    if( !file.open( QIODevice::ReadOnly ) || file.size() < 1
        || file.size() > 1024 * 1024 )
      return;
    QByteArray const content = file.readAll();
    if( content.size() != file.size()
        || QCryptographicHash::hash( content, QCryptographicHash::Sha256 ).toHex()
             != catalogHash )
      return;
    QJsonParseError error;
    QJsonDocument const document = QJsonDocument::fromJson( content, &error );
    if( error.error != QJsonParseError::NoError || !document.isObject() )
      return;
    catalog = document.object();
    if( catalog.value( "schema" ).toString()
          != "goldendict-real-management-catalog-v1" )
      catalog = QJsonObject();
  }

  void Start()
  {
    if( catalog.isEmpty() || !SelectDictionaries() )
    {
      Fail( "catalog or dictionary selection" );
      return;
    }
    if( scenario == "clean-discovery" )
    {
      if( !ApplyWorkflow() )
      {
        Fail( "workflow application" );
        return;
      }
      try
      {
        Config::save( configuration );
      }
      catch( ... )
      {
        Fail( "configuration persistence" );
        return;
      }
    }
    if( !CaptureBeforeRescan() )
    {
      Fail( "pre-rescan evidence capture" );
      return;
    }
    if( !QMetaObject::invokeMethod( parent(), "on_rescanFiles_triggered",
                                   Qt::DirectConnection ) )
    {
      Fail( "rescan dispatch" );
      return;
    }
    CaptureAfterRescan();
  }

private:
  struct Selected
  {
    QString alias;
    QString primary;
    Dictionary::Class * dictionary;
  };

  std::vector< sptr< Dictionary::Class > > const & dictionaries;
  Config::Class & configuration;
  QByteArray catalogHash;
  QByteArray conditionsHash;
  QByteArray outputPath;
  QByteArray scenario;
  QJsonObject catalog;
  std::vector< Selected > selected;
  QJsonArray dictionaryEvidence;
  QJsonArray groupEvidence;
  QJsonArray browseEvidence;

  Dictionary::Class * FindDictionary( QString const & relative,
                                      QString * primary ) const
  {
    QByteArray const corpusRoot = qgetenv( "GOLDENDICT_ACCEPTANCE_CORPUS_ROOT" );
    QString const expected = QFileInfo( QDir( QString::fromUtf8( corpusRoot ) )
                                        .filePath( relative ) ).canonicalFilePath();
    if( expected.isEmpty() )
      return 0;
    for( std::vector< sptr< Dictionary::Class > >::const_iterator item = dictionaries.begin();
         item != dictionaries.end(); ++item )
    {
      std::vector< std::string > const & files = ( *item )->getDictionaryFilenames();
      for( std::vector< std::string >::const_iterator file = files.begin();
           file != files.end(); ++file )
      {
        QString const candidate =
          QFileInfo( QString::fromUtf8( file->c_str() ) ).canonicalFilePath();
        if( candidate.compare( expected, Qt::CaseInsensitive ) == 0 )
        {
          *primary = expected;
          return item->get();
        }
      }
    }
    return 0;
  }

  bool SelectDictionaries()
  {
    selected.clear();
    QJsonArray const items = catalog.value( "dictionaries" ).toArray();
    if( items.size() < 2 || items.size() > 16 )
      return false;
    for( QJsonArray::const_iterator item = items.begin(); item != items.end(); ++item )
    {
      QJsonObject const value = item->toObject();
      QString primary;
      Dictionary::Class * dictionary =
        FindDictionary( value.value( "primary_component" ).toString(), &primary );
      if( !dictionary )
        return false;
      Selected found;
      found.alias = value.value( "id" ).toString();
      found.primary = primary;
      found.dictionary = dictionary;
      selected.push_back( found );
    }
    return true;
  }

  Selected const * ByAlias( QString const & alias ) const
  {
    for( std::vector< Selected >::const_iterator item = selected.begin();
         item != selected.end(); ++item )
      if( item->alias == alias )
        return &( *item );
    return 0;
  }

  QString AliasForRuntimeId( QString const & id ) const
  {
    for( std::vector< Selected >::const_iterator item = selected.begin();
         item != selected.end(); ++item )
      if( QString::fromUtf8( item->dictionary->getId().c_str() ) == id )
        return item->alias;
    return QString();
  }

  bool ResolveReferences( QJsonArray const & aliases,
                          QVector< Config::DictionaryRef > * references ) const
  {
    references->clear();
    for( QJsonArray::const_iterator alias = aliases.begin(); alias != aliases.end(); ++alias )
    {
      Selected const * dictionary = ByAlias( alias->toString() );
      if( !dictionary )
        return false;
      references->push_back( Config::DictionaryRef(
        QString::fromUtf8( dictionary->dictionary->getId().c_str() ),
        QString::fromUtf8( dictionary->dictionary->getName().c_str() ) ) );
    }
    return true;
  }

  bool ResolveMuted( QJsonArray const & aliases,
                     Config::MutedDictionaries * muted ) const
  {
    muted->clear();
    for( QJsonArray::const_iterator alias = aliases.begin(); alias != aliases.end(); ++alias )
    {
      Selected const * dictionary = ByAlias( alias->toString() );
      if( !dictionary )
        return false;
      muted->insert( QString::fromUtf8( dictionary->dictionary->getId().c_str() ) );
    }
    return true;
  }

  bool ApplyWorkflow()
  {
    configuration.groups.clear();
    configuration.groups.nextId = 1;
    QJsonArray const groups =
      catalog.value( "workflow" ).toObject().value( "groups" ).toArray();
    for( QJsonArray::const_iterator item = groups.begin(); item != groups.end(); ++item )
    {
      QJsonObject const value = item->toObject();
      Config::Group group;
      group.id = static_cast< unsigned >( value.value( "id" ).toInt() );
      group.name = value.value( "name" ).toString();
      if( !ResolveReferences( value.value( "dictionary_ids" ).toArray(),
                              &group.dictionaries )
          || !ResolveMuted( value.value( "muted_dictionary_ids" ).toArray(),
                            &group.mutedDictionaries )
          || !ResolveMuted( value.value( "popup_muted_dictionary_ids" ).toArray(),
                            &group.popupMutedDictionaries ) )
        return false;
      configuration.groups.push_back( group );
      configuration.groups.nextId = qMax( configuration.groups.nextId,
                                           group.id + 1 );
    }
    return !configuration.groups.isEmpty();
  }

  QJsonArray AliasArray( QVector< Config::DictionaryRef > const & values ) const
  {
    QJsonArray result;
    for( QVector< Config::DictionaryRef >::const_iterator value = values.begin();
         value != values.end(); ++value )
    {
      QString const alias = AliasForRuntimeId( value->id );
      if( alias.isEmpty() )
        return QJsonArray();
      result.append( alias );
    }
    return result;
  }

  QJsonArray AliasArray( Config::MutedDictionaries const & values,
                         QVector< Config::DictionaryRef > const & members ) const
  {
    QJsonArray result;
    for( QVector< Config::DictionaryRef >::const_iterator member = members.begin();
         member != members.end(); ++member )
    {
      if( !values.contains( member->id ) )
        continue;
      QString const alias = AliasForRuntimeId( member->id );
      if( alias.isEmpty() )
        return QJsonArray();
      result.append( alias );
    }
    return result;
  }

  bool CaptureGroups()
  {
    groupEvidence = QJsonArray();
    for( Config::Groups::const_iterator group = configuration.groups.begin();
         group != configuration.groups.end(); ++group )
    {
      QJsonArray const members = AliasArray( group->dictionaries );
      if( members.size() != group->dictionaries.size() )
        return false;
      QJsonObject value;
      value.insert( "dictionary_ids", members );
      value.insert( "id", static_cast< qint64 >( group->id ) );
      value.insert( "muted_dictionary_ids",
                    AliasArray( group->mutedDictionaries, group->dictionaries ) );
      value.insert( "name", group->name );
      value.insert( "popup_muted_dictionary_ids",
                    AliasArray( group->popupMutedDictionaries,
                                group->dictionaries ) );
      groupEvidence.append( value );
    }
    return true;
  }

  bool CaptureDictionaries()
  {
    dictionaryEvidence = QJsonArray();
    for( std::vector< Selected >::const_iterator item = selected.begin();
         item != selected.end(); ++item )
    {
      QJsonObject value;
      value.insert( "article_count",
                    static_cast< qint64 >( item->dictionary->getArticleCount() ) );
      value.insert( "catalog_id", item->alias );
      QJsonArray components;
      components.append( item->primary );
      value.insert( "components", components );
      QString const description = item->dictionary->getDescription();
      value.insert( "description", description == "NONE" ? QString() : description );
      value.insert( "headword_count",
                    static_cast< qint64 >( item->dictionary->getWordCount() ) );
      value.insert( "name", QString::fromUtf8( item->dictionary->getName().c_str() ) );
      value.insert( "source_language", LangCoder::intToCode2( item->dictionary->getLangFrom() ) );
      value.insert( "target_language", LangCoder::intToCode2( item->dictionary->getLangTo() ) );
      dictionaryEvidence.append( value );
    }
    return true;
  }

  bool CaptureBrowse()
  {
    browseEvidence = QJsonArray();
    QJsonArray const requests =
      catalog.value( "workflow" ).toObject().value( "browse" ).toArray();
    for( QJsonArray::const_iterator request = requests.begin();
         request != requests.end(); ++request )
    {
      QJsonObject const value = request->toObject();
      Selected const * dictionary = ByAlias( value.value( "dictionary_id" ).toString() );
      int const maximum = value.value( "page_size" ).toInt() * 2;
      QStringList headwords;
      if( !dictionary || maximum < 2 || !dictionary->dictionary->getHeadwords( headwords )
          || headwords.isEmpty() )
        return false;
      headwords.sort( Qt::CaseSensitive );
      QJsonArray bounded;
      for( int index = 0; index < headwords.size() && index < maximum; ++index )
        bounded.append( headwords.at( index ) );
      QJsonObject evidence;
      evidence.insert( "dictionary_id", dictionary->alias );
      evidence.insert( "headwords", bounded );
      browseEvidence.append( evidence );
    }
    return true;
  }

  bool CaptureBeforeRescan()
  {
    return CaptureDictionaries() && CaptureGroups() && CaptureBrowse();
  }

  void CaptureAfterRescan()
  {
    if( !SelectDictionaries() )
    {
      Fail( "post-rescan dictionary selection" );
      return;
    }
    QJsonArray rescanned;
    for( std::vector< Selected >::const_iterator item = selected.begin();
         item != selected.end(); ++item )
      rescanned.append( item->alias );
    QJsonObject raw;
    raw.insert( "browse", browseEvidence );
    raw.insert( "catalog_sha256", QString::fromLatin1( catalogHash ) );
    raw.insert( "conditions_sha256", QString::fromLatin1( conditionsHash ) );
    raw.insert( "dictionaries", dictionaryEvidence );
    raw.insert( "groups", groupEvidence );
    raw.insert( "rescan_dictionary_ids", rescanned );
    raw.insert( "scenario", QString::fromLatin1( scenario ) );
    raw.insert( "schema", "goldendict-real-management-raw-observation-v1" );
    QSaveFile output( QString::fromUtf8( outputPath ) );
    if( !output.open( QIODevice::WriteOnly ) )
    {
      Fail( "output open" );
      return;
    }
    QByteArray const content =
      QJsonDocument( raw ).toJson( QJsonDocument::Compact ) + '\n';
    if( output.write( content ) != content.size() || !output.commit() )
    {
      Fail( "output commit" );
      return;
    }
    ScheduleExit();
    deleteLater();
  }

  void Fail( char const * reason )
  {
    QFile diagnostic( QString::fromUtf8( outputPath ) + ".failure" );
    if( diagnostic.open( QIODevice::WriteOnly ) )
    {
      diagnostic.write( reason );
      diagnostic.write( "\n" );
      diagnostic.close();
    }
    std::fprintf( stderr, "Qt 5 management observation failed: %s\n", reason );
    std::fflush( stderr );
    qApp->exit( 4 );
    deleteLater();
  }
};

void PublishManagement( QObject * owner,
                        std::vector< sptr< Dictionary::Class > > const & dictionaries,
                        Config::Class & configuration,
                        QByteArray const & catalogPath,
                        QByteArray const & catalogHash,
                        QByteArray const & conditionsHash,
                        QByteArray const & outputPath,
                        QByteArray const & scenario )
{
  ManagementPublication * publication = new ManagementPublication(
    owner, dictionaries, configuration, catalogPath, catalogHash,
    conditionsHash, outputPath, scenario );
  QTimer::singleShot( 0, publication, [publication]() { publication->Start(); } );
}

} // unnamed namespace

void Publish( QObject * owner,
              std::vector< sptr< Dictionary::Class > > const & dictionaries,
              FTS::FtsIndexing & ftsIndexing,
              Config::Class & configuration )
{
  QByteArray const outputPath = qgetenv( "GOLDENDICT_ACCEPTANCE_RAW_RESULT_PATH" );
  if( outputPath.isEmpty() )
    return;

  QByteArray const conditionsHash = qgetenv( "GOLDENDICT_ACCEPTANCE_CONDITIONS_SHA256" );
  QByteArray const scenario = qgetenv( "GOLDENDICT_ACCEPTANCE_SCENARIO" );
  if( conditionsHash.isEmpty()
      || ( scenario != "clean-discovery" && scenario != "warm-restart"
           && scenario != "explicit-rescan" && scenario != "changed-source"
           && scenario != "cancellation" && scenario != "cancellation-recovery"
           && scenario != "unavailable-companion"
           && scenario != "companion-recovery" ) )
  {
    ScheduleExit();
    return;
  }

  QByteArray const mdictCatalog = qgetenv( "GOLDENDICT_ACCEPTANCE_MDICT_CATALOG" );
  if( !mdictCatalog.isEmpty() )
  {
    static bool mdictStarted = false;
    QByteArray const mdictCatalogHash =
      qgetenv( "GOLDENDICT_ACCEPTANCE_MDICT_CATALOG_SHA256" );
    if( mdictStarted || mdictCatalogHash.size() != 64 )
    {
      ScheduleExit();
      return;
    }
    mdictStarted = true;
    PublishMdict( owner, dictionaries, mdictCatalog, mdictCatalogHash,
                  conditionsHash, outputPath, scenario );
    return;
  }

  QByteArray const managementCatalog =
    qgetenv( "GOLDENDICT_ACCEPTANCE_MANAGEMENT_CATALOG" );
  if( !managementCatalog.isEmpty() )
  {
    static bool managementStarted = false;
    QByteArray const managementCatalogHash =
      qgetenv( "GOLDENDICT_ACCEPTANCE_MANAGEMENT_CATALOG_SHA256" );
    if( managementStarted || managementCatalogHash.size() != 64 )
    {
      ScheduleExit();
      return;
    }
    managementStarted = true;
    PublishManagement( owner, dictionaries, configuration, managementCatalog,
                       managementCatalogHash, conditionsHash, outputPath,
                       scenario );
    return;
  }

  QByteArray const lookupCatalog = qgetenv( "GOLDENDICT_ACCEPTANCE_LOOKUP_CATALOG" );
  if( !lookupCatalog.isEmpty() )
  {
    static bool lookupStarted = false;
    QByteArray const lookupCatalogHash =
      qgetenv( "GOLDENDICT_ACCEPTANCE_LOOKUP_CATALOG_SHA256" );
    if( lookupStarted || lookupCatalogHash.size() != 64 )
    {
      ScheduleExit();
      return;
    }
    lookupStarted = true;
    PublishLookup( owner, dictionaries, lookupCatalog, lookupCatalogHash,
                   conditionsHash, outputPath, scenario );
    return;
  }

  QByteArray const dslCatalog = qgetenv( "GOLDENDICT_ACCEPTANCE_DSL_CATALOG" );
  if( !dslCatalog.isEmpty() )
  {
    static bool dslStarted = false;
    QByteArray const dslCatalogHash =
      qgetenv( "GOLDENDICT_ACCEPTANCE_DSL_CATALOG_SHA256" );
    if( dslStarted || dslCatalogHash.size() != 64 )
    {
      ScheduleExit();
      return;
    }
    dslStarted = true;
    PublishDsl( owner, dictionaries, dslCatalog, dslCatalogHash,
                conditionsHash, outputPath, scenario );
    return;
  }

  static bool rescanRequested = false;
  if( scenario == "explicit-rescan" && !rescanRequested )
  {
    rescanRequested = true;
    if( !QMetaObject::invokeMethod( owner, "on_rescanFiles_triggered",
                                   Qt::QueuedConnection ) )
      ScheduleExit();
    return;
  }

  if( scenario == "cancellation" || scenario == "cancellation-recovery" )
  {
    QTimer * timer = new QTimer( owner );
    std::shared_ptr< QElapsedTimer > elapsed( new QElapsedTimer );
    std::shared_ptr< bool > started( new bool( false ) );
    std::shared_ptr< std::string > dictionaryId( new std::string );
    elapsed->start();
    QObject::connect( timer, &QTimer::timeout, owner,
                      [=, &dictionaries, &ftsIndexing]() {
      QString const indexingName = ftsIndexing.nowIndexingName();
      if( !*started && !indexingName.isEmpty() )
      {
        Dictionary::Class * dictionary = FindIndexingDictionary( dictionaries, indexingName );
        if( !dictionary
            || ( scenario == "cancellation" && dictionary->haveFTSIndex() ) )
        {
          timer->stop();
          qApp->exit( 4 );
          return;
        }
        *started = true;
        *dictionaryId = dictionary->getId();
        if( scenario == "cancellation" )
        {
          ftsIndexing.stopIndexing();
          if( dictionary->haveFTSIndex() )
          {
            timer->stop();
            qApp->exit( 4 );
            return;
          }
          QJsonArray phases;
          phases.append( Phase( "full-text-indexing", "started", 0, *dictionaryId ) );
          phases.append( Phase( "full-text-indexing", "cancelled", 1, *dictionaryId ) );
          timer->stop();
          if( !WriteRaw( outputPath, conditionsHash, scenario, Catalog( dictionaries ),
                         phases, "cancelled" ) )
          {
            qApp->exit( 4 );
            return;
          }
          ScheduleExit();
        }
        return;
      }
      if( scenario == "cancellation-recovery" && *started && indexingName.isEmpty() )
      {
        Dictionary::Class * dictionary = 0;
        for( std::vector< sptr< Dictionary::Class > >::const_iterator item = dictionaries.begin();
             item != dictionaries.end(); ++item )
          if( ( *item )->getId() == *dictionaryId )
            dictionary = item->get();
        if( !dictionary || !dictionary->haveFTSIndex() )
        {
          timer->stop();
          qApp->exit( 4 );
          return;
        }
        QJsonArray phases;
        phases.append( Phase( "full-text-indexing", "started", 0, *dictionaryId ) );
        phases.append( Phase( "full-text-indexing", "completed", 1, *dictionaryId ) );
        timer->stop();
        if( !WriteRaw( outputPath, conditionsHash, scenario, Catalog( dictionaries ),
                       phases, "completed" ) )
        {
          qApp->exit( 4 );
          return;
        }
        ScheduleExit();
        return;
      }
      if( elapsed->elapsed() > kIndexingTimeoutMilliseconds )
      {
        timer->stop();
        qApp->exit( 4 );
      }
    } );
    timer->start( 1 );
    return;
  }

  QJsonArray const catalog = Catalog( dictionaries );

  QJsonArray phases;
  QJsonObject started;
  started.insert( "dictionary_id", QString() );
  QByteArray const phaseName = scenario == "warm-restart" ? "restart" :
                               scenario == "explicit-rescan" ? "rescan" :
                               scenario == "changed-source" ? "source-change" :
                               scenario == "unavailable-companion" ? "companion-unavailable" :
                               scenario == "companion-recovery" ? "companion-recovery" :
                               "discovery";
  started.insert( "name", QString::fromLatin1( phaseName ) );
  started.insert( "sequence", 0 );
  started.insert( "status", "started" );
  phases.append( started );
  QJsonObject completed;
  completed.insert( "dictionary_id", QString() );
  completed.insert( "name", QString::fromLatin1( phaseName ) );
  completed.insert( "sequence", 1 );
  completed.insert( "status", "completed" );
  phases.append( completed );

  WriteRaw( outputPath, conditionsHash, scenario, catalog, phases, "completed" );
  ScheduleExit();
}

} // namespace Qt5AcceptanceObserver
