"""Calculates DocHits."""

import argparse
import json
import sys
import requests


def getparser():
    """Gets program's parser."""
    parser = argparse.ArgumentParser(
        prog='run', description='Client for Bloodhound server that calculates'
                                'dochits.')

    parser.add_argument('url', help='The URL of the server.')
    parser.add_argument('infile', nargs='?', type=argparse.FileType('r'),
                        default=sys.stdin, help='Input file (optional).')
    parser.add_argument('-k', nargs='?', type=int, default=30)
    parser.add_argument('--skip-header', action='store_true', default=False)
    return parser


def main():
    """Main function."""
    parser = getparser()
    args = parser.parse_args()
    if not args.skip_header:
        print('doc', 'hits', sep=',')
    dochits = {}
    for query in args.infile:
        query = query[:-1]
        if '\t' in query:
            _, query = query.split('\t')
        request_data = {
            'type': 'taat',
            'k': args.k,
            'query': query
        }
        response = requests.post(args.url, json=request_data)
        resp_json = json.loads(response.text)
        for result in resp_json['results']:
            doc = result['doc']
            hits = dochits.get(doc, 0) + 1
            dochits[doc] = hits
    for doc, hits in dochits.items():
        print(doc, hits, sep=',')


if __name__ == '__main__':
    main()
