"""Runs queries and reports times."""

import argparse
import json
import sys
import requests


def getparser():
    """Gets program's parser."""
    parser = argparse.ArgumentParser(
        prog='run', description='Client for Bloodhound server.')

    parser.add_argument('url', help='The URL of the server.')
    parser.add_argument('infile', nargs='?', type=argparse.FileType('r'),
                        default=sys.stdin, help='Input file (optional).')
    parser.add_argument('-r', '--retriever', nargs='?',
                        choices=['taat', 'taat+', 'daat', 'wand', 'mscore',
                                 'saat', 'asaat', 'rtaat', 'tmscore', 'ness'],
                        default='taat', help='Type of retriever.')
    parser.add_argument('--et', nargs='?', type=float, default=1.0)
    parser.add_argument('-k', nargs='?', type=int, default=30)

    subparsers = parser.add_subparsers(dest='cmd', help='Script command')

    time_parser = subparsers.add_parser(
        'time', help='Report query times.')
    unit_group = time_parser.add_mutually_exclusive_group()
    unit_group.add_argument('--milli', action='store_true',
                            default=False, help='Print milliseconds.')
    unit_group.add_argument('--micro', action='store_true',
                            default=False, help='Print microseconds.')

    results_parser = subparsers.add_parser('results',
                                           help='Report query results.')
    results_parser.add_argument('--titles', nargs='?',
                                type=argparse.FileType('r'), default=None,
                                help='Document titles file .')
    subparsers.add_parser(
        'thresholds', help='Report query thresholds for top-k.')

    return parser


def main():
    """Main function."""
    parser = getparser()
    args = parser.parse_args()
    titles = []
    if args.cmd == 'results' and args.titles:
        for title in args.titles:
            titles.append(title[:-1])
    fulltime = 0
    nqueries = 0
    for query in args.infile:
        query = query[:-1]
        img = None
        if '\t' in query:
            img, query = query.split('\t')
        request_data = {
            'type': args.retriever,
            'k': args.k,
            'query': query
        }
        if args.retriever in {'saat', 'asaat'}:
            assert 0 < args.et <= 1.0, 'et must be in (0, 1]'
            request_data['saat_et_threshold'] = args.et
        response = requests.post(args.url, json=request_data)
        resp_json = json.loads(response.text)
        if args.cmd == 'results':
            if img:
                print(img, end='\t')
            if args.titles:
                print(' '.join([titles[result['doc']]
                                for result in resp_json['results']]))
            else:
                print(' '.join([str(result['doc'])
                                for result in resp_json['results']]))
        elif args.cmd == 'time':
            nqueries += 1
            nanoseconds = resp_json['nanoseconds']
            fulltime += nanoseconds / 1000000
            if args.milli:
                print(nanoseconds / 1000000)
            elif args.micro:
                print(nanoseconds / 1000)
            else:
                print(nanoseconds)
        elif args.cmd == 'thresholds':
            print(resp_json['results'][-1]['score'])
    if args.cmd == 'time':
        print('Avg ms:', fulltime / nqueries, file=sys.stderr)


if __name__ == '__main__':
    main()
